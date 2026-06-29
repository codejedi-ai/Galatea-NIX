/*
 * Interactive serial shell for the kernel.
 *
 * A simple in-kernel "terminal": it reads a line from the console UART and
 * dispatches to built-in commands. It cooperates with the scheduler by polling
 * the UART receive flag and Yield()-ing when no input is available, so other
 * tasks keep running. This is the seed of userland -- once the OS gains an ELF
 * loader and a filesystem (see docs/OS_ROADMAP.md), commands like `run` and an
 * editor become external programs instead of built-ins.
 */
#include "rpi.h"
#include "syscall.h"
#include "malloc.h"
#include "string.h"
#include "pmm.h"
#include "vmm.h"
#include "project.h"
#include "clockserver.h"
#include "clock_client.h"
#include "rps.h"
#include "nameserver.h"
#include "../layer3-services/uart/io_api/io_api.h"
#include "../layer3-services/uart/UART1_CONSOLE_server/UART1_CONSOLE_server.h"
#include "layer4_ui.h"
#include "display_client.h"
#include "displayserver.h"
#include "ui_elem.h"
#include "ui_app.h"
#include "ui_screen.h"
#include "apps.h"
#include "ramfs.h"
#include "diskfs.h"
#include "config.h"
#include "accel.h"
#include "../layer3-services/idle.h"
#if ENABLE_LINK == 1
#include "linkserver.h"
#endif
#if ENABLE_MARKLIN == 1
#include "marklin.h"
#include "tc1.h"
#endif

#define SHELL_LINE_MAX 128
#define SHELL_INNER_COL  1   /* 0-based: first column inside DIV │ border */

static UiAppShell g_shell_frame;

static void shell_prompt_print(const char *cwd);

static int shell_body_bottom(int rows)
{
	return rows - 3;  /* above DIV status + bottom border */
}

static void shell_write(const char *s)
{
	if (DisplayServerTid() >= 0)
		display_write_str(s);
	else
		uart_puts(CONSOLE, (char *)s);
}

static void shell_frame_draw(const char *status)
{
	int rows, cols;

	display_get_size(&rows, &cols);
	ui_app_shell_init(&g_shell_frame, "Terminal",
	                  status ? status : "Type 'help'",
	                  rows, 0, 0);
	ui_app_shell_render(&g_shell_frame, 1);
	display_shell_region(UI_APP_CONTENT_ROW, shell_body_bottom(rows));
	display_goto_cursor(UI_APP_CONTENT_ROW, SHELL_INNER_COL);
}

static void shell_redraw_line(const char *cwd, const char *buf, int len)
{
	int row, col;

	display_get_cursor(&row, &col, 0);
	display_goto_cursor(row, SHELL_INNER_COL);
	if (cwd) shell_prompt_print(cwd);
	for (int i = 0; i < len; i++)
		display_readline_echo(buf[i]);
	shell_write("\033[K");
}

static void shell_prompt_print(const char *cwd)
{
	char line[96];
	int p = 0;

	line[p++] = '\033'; line[p++] = '['; line[p++] = '1'; line[p++] = ';';
	line[p++] = '3'; line[p++] = '2'; line[p++] = 'm';
	for (const char *s = PROJECT_USER; *s && p < (int)sizeof(line) - 24; s++)
		line[p++] = *s;
	line[p++] = '\033'; line[p++] = '['; line[p++] = '0'; line[p++] = 'm';
	line[p++] = '@';
	line[p++] = '\033'; line[p++] = '['; line[p++] = '1'; line[p++] = ';';
	line[p++] = '3'; line[p++] = '6'; line[p++] = 'm';
	for (const char *s = PROJECT_HOSTNAME; *s && p < (int)sizeof(line) - 16; s++)
		line[p++] = *s;
	line[p++] = '\033'; line[p++] = '['; line[p++] = '0'; line[p++] = 'm';
	line[p++] = ':';
	line[p++] = '\033'; line[p++] = '['; line[p++] = '1'; line[p++] = ';';
	line[p++] = '3'; line[p++] = '4'; line[p++] = 'm';
	for (const char *s = cwd; *s && p < (int)sizeof(line) - 8; s++)
		line[p++] = *s;
	for (const char *s = "\033[0m$ "; *s && p < (int)sizeof(line) - 1; s++)
		line[p++] = *s;
	line[p] = 0;
	shell_write(line);
}

/* Read one line from the console (cooperative: Yield while idle). Handles
 * backspace and ignores other control characters. NUL-terminates buf.
 * cwd: path for prompt, or NULL for a bare line (nano).
 * Empty string (len==0) means no command. */
static void shell_readline(char *buf, int max, const char *cwd)
{
	int len = 0;
	int clock_tid = ClockServerTid();

	buf[0] = '\0';
	for (;;) {
		unsigned char c;
		int console_tid = WhoIs(UART1_CONSOLE_SERVER);
		if (console_tid > 0) {
			c = (unsigned char)ConsoleGetc(console_tid);
		} else {
			while (!uart_rxc(CONSOLE)) {
				if (clock_tid >= 0)
					Delay(clock_tid, 1);
				else
					Yield();
			}
			c = uart_getc(CONSOLE);
		}

		if (c == '\r' || c == '\n') {        /* end of line */
			shell_write("\r\n");
			display_readline_end();
			break;
		}
		if (c == 0x7f || c == 0x08) {        /* backspace: drop last char */
			if (len > 0) {
				len--;
				buf[len] = '\0';
				if (DisplayServerTid() >= 0)
					display_erase_char();
				else
					shell_redraw_line(cwd, buf, len);
			}
			continue;
		}
		if (c < 0x20)                        /* ignore other control chars */
			continue;
		if (len == 0 && (c == ' ' || c == '\t'))  /* no leading whitespace */
			continue;
		if (len < max - 1) {
			buf[len++] = (char)c;
			if (DisplayServerTid() >= 0)
				display_readline_echo((char)c);
			else
				uart_putc(CONSOLE, c);
		}
	}
	buf[len] = '\0';
}

/* Split "cmd arg arg..." into the command token and the remaining argument
 * string (trimmed of the single separating space). Mutates line. */
static char *split_args(char *line)
{
	char *p = line;
	while (*p && *p != ' ')
		p++;
	if (*p == '\0')
		return p;            /* no args: points at the NUL */
	*p = '\0';
	return p + 1;            /* args start after the space */
}

static void cmd_help(void)
{
	uart_printf(CONSOLE, "Built-in commands:\r\n");
	uart_printf(CONSOLE, "  help          show this help\r\n");
	uart_printf(CONSOLE, "  echo <text>   print text\r\n");
	uart_printf(CONSOLE, "  mem           heap usage (total/used/free)\r\n");
	uart_printf(CONSOLE, "  pages         physical frame pool\r\n");
	uart_printf(CONSOLE, "  run           launch a demo C program as a task\r\n");
	uart_printf(CONSOLE, "  uptime        kernel runtime in ticks\r\n");
	uart_printf(CONSOLE, "  tid           this shell's task id\r\n");
	uart_printf(CONSOLE, "  clear         clear the screen\r\n");
	uart_printf(CONSOLE, "  about         about this OS\r\n");
	uart_printf(CONSOLE, "Apps:\r\n");
	uart_printf(CONSOLE, "  rps           Rock-Paper-Scissors vs the computer\r\n");
	uart_printf(CONSOLE, "  snake         Snake\r\n");
	uart_printf(CONSOLE, "  tetris        Tetris\r\n");
	uart_printf(CONSOLE, "System:\r\n");
	uart_printf(CONSOLE, "  neofetch      system info splash\r\n");
	uart_printf(CONSOLE, "  htop          live system monitor\r\n");
	uart_printf(CONSOLE, "  screen        set/show resolution (auto|WxH|preset)\r\n");
#if ENABLE_LINK == 1
	uart_printf(CONSOLE, "Outside world (container link):\r\n");
	uart_printf(CONSOLE, "  link <cmd>    talk to container hw (try: link PING)\r\n");
	uart_printf(CONSOLE, "  linktest      verify the Marklin mock (handshake)\r\n");
#endif
#if ENABLE_MARKLIN == 1
	uart_printf(CONSOLE, "Marklin train control:\r\n");
	uart_printf(CONSOLE, "  tr <addr> <0-15>   set train speed (0=stop 15=E-stop)\r\n");
	uart_printf(CONSOLE, "  sw <addr> <S|C>    throw a switch (S=straight C=curved)\r\n");
	uart_printf(CONSOLE, "  sensors            poll and display all 80 sensor states\r\n");
	uart_printf(CONSOLE, "  tc1 start          start train controller 1 (broadcasts to web)\r\n");
	uart_printf(CONSOLE, "  tc1 stop           stop train controller 1\r\n");
	uart_printf(CONSOLE, "  tc1 speed <0-14>   set TC1 train speed\r\n");
	uart_printf(CONSOLE, "  map [A|B]          show ASCII track schematic (A=Track A, B=Track B)\r\n");
#endif
	uart_printf(CONSOLE, "Files (RAM = /  |  Persistent = /disk/):\r\n");
	uart_printf(CONSOLE, "  ls [dir]  cd <dir>  pwd  tree\r\n");
	uart_printf(CONSOLE, "  mkdir <d>  cat <f>  rm <p>\r\n");
	uart_printf(CONSOLE, "  write <f> <text>   nano <f>\r\n");
	uart_printf(CONSOLE, "  (prefix path with /disk/ for persistent storage)\r\n");
}

static void cmd_mem(void)
{
	uint64_t total = malloc_total_bytes();
	uint64_t freeb = malloc_free_bytes();
	uint64_t used  = (total >= freeb) ? (total - freeb) : 0;
	uint64_t blocks = malloc_free_blocks();
	uart_printf(CONSOLE, "heap: total=%u B  used=%u B  free=%u B  free-blocks=%u\r\n",
	            (unsigned)total, (unsigned)used, (unsigned)freeb, (unsigned)blocks);
}

static void cmd_about(void)
{
	uart_printf(CONSOLE, "\033[1;36m%s\033[0m@%s -- ultralight AArch64 microkernel\r\n",
	            PROJECT_USER, PROJECT_HOSTNAME);
	uart_printf(CONSOLE, "IPC: Send/Receive/Reply (QNX-style). Tasks share memory.\r\n");
	uart_printf(CONSOLE, "Roadmap: docs/OS_ROADMAP.md\r\n");
}

/* A small C "program" that runs as its own EL0 task: does some computation,
 * makes a syscall (MyTid), uses the heap, prints, then Exit()s. This is what a
 * compiled-in user program looks like in the current (no-loader) model. */
static void demo_program(void)
{
	uart_printf(CONSOLE, "  [demo] hello from a C program running at EL0! tid=%d\r\n", MyTid());

	uint64_t f = 1;
	for (int i = 1; i <= 10; i++) f *= (uint64_t)i;
	uart_printf(CONSOLE, "  [demo] computed 10! = %u\r\n", (unsigned)f);

	uint64_t *p = mymalloc(4);   /* 4 words from the kernel heap */
	if (p) {
		p[0] = 0xABCD1234u;
		uart_printf(CONSOLE, "  [demo] heap alloc OK, wrote/read 0x%x\r\n", (unsigned)p[0]);
		myfree(p);
	}
	uart_printf(CONSOLE, "  [demo] done; exiting.\r\n");
	Exit();
}

/* ===================== RAM filesystem + tools (commands) ================= */

static char cwd[RAMFS_PATH_MAX] = "/";

static void s_cpy(char *d, const char *s) { int i = 0; for (; s[i]; i++) d[i] = s[i]; d[i] = 0; }

static void print_text(const char *d, int len)
{
	for (int i = 0; i < len; i++) {
		if (d[i] == '\n') uart_printf(CONSOLE, "\r\n");
		else uart_putc(CONSOLE, (unsigned char)d[i]);
	}
}

static void path_resolve(const char *arg, char *out)   /* arg vs cwd -> absolute */
{
	int p = 0;
	if (!arg || !arg[0]) { s_cpy(out, cwd); return; }
	if (arg[0] == '/') {
		for (int i = 0; arg[i] && p < RAMFS_PATH_MAX - 1; i++) out[p++] = arg[i];
	} else {
		for (int i = 0; cwd[i] && p < RAMFS_PATH_MAX - 1; i++) out[p++] = cwd[i];
		if (p == 0 || out[p - 1] != '/') out[p++] = '/';
		for (int i = 0; arg[i] && p < RAMFS_PATH_MAX - 1; i++) out[p++] = arg[i];
	}
	out[p] = 0;
}

static void ls_cb(const char *name, int is_dir, int size, void *u)
{
	(void)u;
	if (is_dir) uart_printf(CONSOLE, "  \033[1;34m%s/\033[0m\r\n", name);
	else        uart_printf(CONSOLE, "  %s  (%d B)\r\n", name, size);
}

/* Returns 1 if path begins with /disk (persistent layer), 0 for RAM layer */
static int is_disk_path(const char *p)
{
	return p[0]=='/' && p[1]=='d' && p[2]=='i' && p[3]=='s' && p[4]=='k' &&
	       (p[5]=='/' || p[5]==0);
}

/* Print disk directory listing for path */
static void disk_ls_print(const char *path)
{
	char buf[DISKFS_BUF_MAX];
	int n = DiskLs(path, buf, (int)sizeof(buf));
	if (n == DISKFS_NOENT) { uart_printf(CONSOLE, "ls: no such directory: %s\r\n", path); return; }
	if (n == DISKFS_ERR)   { uart_printf(CONSOLE, "ls: link error\r\n"); return; }
	if (n == 0)            { uart_printf(CONSOLE, "(empty)\r\n"); return; }
	/* print space-separated list as one-per-line with type indicator */
	char *p = buf;
	while (*p) {
		char name[64]; int ni = 0;
		while (*p && *p != ' ' && ni < 63) name[ni++] = *p++;
		name[ni] = 0;
		while (*p == ' ') p++;
		/* build full child path to stat it */
		char cp[DISKFS_PATH_MAX];
		int bl = 0;
		for (int i = 0; path[i] && bl < DISKFS_PATH_MAX-2; i++) cp[bl++] = path[i];
		if (cp[bl-1] != '/') cp[bl++] = '/';
		for (int i = 0; name[i] && bl < DISKFS_PATH_MAX-1; i++) cp[bl++] = name[i];
		cp[bl] = 0;
		int sz; int st = DiskStat(cp, &sz);
		if (st == 2) uart_printf(CONSOLE, "\033[1;34m%s/\033[0m\r\n", name);
		else         uart_printf(CONSOLE, "%s  (%d B)\r\n", name, sz);
	}
}

static void cmd_ls(const char *args)
{
	char path[RAMFS_PATH_MAX]; path_resolve(args, path);
	if (is_disk_path(path)) { disk_ls_print(path); return; }
	ramfs_list(path, ls_cb, 0);
}

static void cmd_cd(const char *args)
{
	if (!args[0] || (args[0] == '/' && !args[1])) { cwd[0] = '/'; cwd[1] = 0; return; }
	if (args[0] == '.' && args[1] == '.' && !args[2]) {
		int last = 0; for (int i = 0; cwd[i]; i++) if (cwd[i] == '/') last = i;
		cwd[last == 0 ? 1 : last] = 0; return;
	}
	char path[RAMFS_PATH_MAX]; path_resolve(args, path);
	if (is_disk_path(path)) {
		int st = DiskStat(path, 0);
		if (st == 2) { s_cpy(cwd, path); return; }
		if (st == 0 && path[5] == 0) { s_cpy(cwd, path); return; } /* /disk itself */
		uart_printf(CONSOLE, "cd: no such directory: %s\r\n", path);
	} else {
		if (ramfs_exists(path) == 2) s_cpy(cwd, path);
		else uart_printf(CONSOLE, "cd: no such folder: %s\r\n", path);
	}
}

static void cmd_cat(const char *args)
{
	char path[RAMFS_PATH_MAX]; path_resolve(args, path);
	if (is_disk_path(path)) {
		static char dbuf[512];
		int n = DiskRead(path, dbuf, (int)sizeof(dbuf));
		if (n == DISKFS_NOENT) { uart_printf(CONSOLE, "cat: not found: %s\r\n", path); return; }
		if (n == DISKFS_ERR)   { uart_printf(CONSOLE, "cat: link error\r\n"); return; }
		print_text(dbuf, n);
		uart_printf(CONSOLE, "\r\n");
		return;
	}
	int len; const char *d = ramfs_read(path, &len);
	if (!d) { uart_printf(CONSOLE, "cat: not a file: %s\r\n", path); return; }
	print_text(d, len);
	uart_printf(CONSOLE, "\r\n");
}

static void cmd_mkdir(const char *args)
{
	if (!args[0]) { uart_printf(CONSOLE, "usage: mkdir <name>\r\n"); return; }
	char path[RAMFS_PATH_MAX]; path_resolve(args, path);
	if (is_disk_path(path)) {
		if (DiskMkdir(path) < 0) uart_printf(CONSOLE, "mkdir: link error: %s\r\n", path);
		return;
	}
	if (ramfs_mkdir(path) < 0) uart_printf(CONSOLE, "mkdir failed: %s\r\n", path);
}

static void cmd_rm(const char *args)
{
	if (!args[0]) { uart_printf(CONSOLE, "usage: rm <name>\r\n"); return; }
	char path[RAMFS_PATH_MAX]; path_resolve(args, path);
	if (is_disk_path(path)) {
		int r = DiskRm(path);
		if (r == DISKFS_NOENT) uart_printf(CONSOLE, "rm: not found: %s\r\n", path);
		else if (r == DISKFS_ERR) uart_printf(CONSOLE, "rm: link error\r\n");
		return;
	}
	if (ramfs_remove(path) < 0) uart_printf(CONSOLE, "rm: not found: %s\r\n", path);
}

static void cmd_write(char *args)   /* write <path> <text...> */
{
	char *text = args;
	while (*text && *text != ' ') text++;
	if (*text == ' ') { *text = 0; text++; }
	if (!args[0]) { uart_printf(CONSOLE, "usage: write <file> <text>\r\n"); return; }
	char path[RAMFS_PATH_MAX]; path_resolve(args, path);
	if (is_disk_path(path)) {
		int tlen = 0; while (text[tlen]) tlen++;
		int r = DiskWrite(path, text, tlen);
		if (r < 0) uart_printf(CONSOLE, "write: link error\r\n");
		return;
	}
	ramfs_write(path, text, -1);
}

static char nano_buf[RAMFS_FILE_CAP];
static void cmd_nano(const char *args)
{
	if (!args[0]) { uart_printf(CONSOLE, "usage: nano <file>\r\n"); return; }
	char path[RAMFS_PATH_MAX]; path_resolve(args, path);
	int el; const char *ex = ramfs_read(path, &el);
	uart_printf(CONSOLE, "-- nano %s --  (a single '.' on a line saves & exits)\r\n", path);
	if (ex) { uart_printf(CONSOLE, "--- current ---\r\n"); print_text(ex, el); uart_printf(CONSOLE, "--- type new content ---\r\n"); }
	int blen = 0;
	char line[120];
	for (;;) {
		display_readline_start();
		shell_readline(line, (int)sizeof(line), NULL);
		if (line[0] == '.' && line[1] == 0) break;
		for (int i = 0; line[i] && blen < RAMFS_FILE_CAP - 2; i++) nano_buf[blen++] = line[i];
		if (blen < RAMFS_FILE_CAP - 1) nano_buf[blen++] = '\n';
	}
	nano_buf[blen] = 0;
	ramfs_write(path, nano_buf, blen);
	uart_printf(CONSOLE, "saved %s (%d B)\r\n", path, blen);
}

static void tree_cb(const char *path, int is_dir, int size, void *u)
{
	(void)u;
	int depth = 0;
	for (int i = 0; path[i]; i++) if (path[i] == '/') depth++;
	for (int i = 1; i < depth; i++) uart_printf(CONSOLE, "  ");
	const char *name = path;
	for (int i = 0; path[i]; i++) if (path[i] == '/') name = &path[i + 1];
	if (is_dir) uart_printf(CONSOLE, "\033[1;34m%s/\033[0m\r\n", name);
	else        uart_printf(CONSOLE, "%s  (%d B)\r\n", name, size);
}

static void cmd_tree(void) { uart_printf(CONSOLE, "/\r\n"); ramfs_each(tree_cb, 0); }

/* ---- helpers for string formatting (uart_printf has no width specifiers) ---- */
static int h_app(char *b, int p, const char *s) { int i = 0; while (s[i]) b[p++] = s[i++]; b[p] = 0; return p; }
static int h_apu(char *b, int p, unsigned v) { char t[12]; int n = app_uitoa(v, t); for (int i = 0; i < n; i++) b[p++] = t[i]; b[p] = 0; return p; }
static int h_ap2(char *b, int p, unsigned v) { if (v < 10) b[p++] = '0'; return h_apu(b, p, v); }
static int h_pad(char *b, int p, int col) { while (p < col) b[p++] = ' '; b[p] = 0; return p; }

/* ===================== splash (neofetch-style) ===================== */

static void cmd_neofetch(void)
{
	unsigned ticks = (unsigned)GetKernelRuntime();
	unsigned ftot  = (unsigned)pmm_total_pages();
	unsigned ffree = (unsigned)pmm_free_pages_count();
	unsigned htot  = (unsigned)malloc_total_bytes();
	unsigned hfree = (unsigned)malloc_free_bytes();
	const char *C = "\033[1;36m", *Y = "\033[1;33m", *R = "\033[0m";

	/* convert ticks (10ms each) to seconds, then to YY MM DD HH MM SS */
	unsigned sec = ticks / 100;
	unsigned days = sec / (24 * 3600);
	unsigned yy = days / 365, mm_days = days % 365;
	unsigned mm = mm_days / 30, dd = mm_days % 30;
	unsigned rem = sec % (24 * 3600);
	unsigned hh = rem / 3600, rem2 = rem % 3600;
	unsigned mm_time = rem2 / 60, ss = rem2 % 60;

	/* convert frames to MB (256 frames = 1 MB) */
	unsigned used_mb = (ftot - ffree) / 256;
	unsigned total_mb = ftot / 256;

	/* format uptime string with zero-padding (uart_printf lacks %02u) */
	char upbuf[32]; int p = 0;
	p = h_ap2(upbuf, p, yy); upbuf[p++] = '-';
	p = h_ap2(upbuf, p, mm); upbuf[p++] = '-';
	p = h_ap2(upbuf, p, dd); upbuf[p++] = ' ';
	p = h_ap2(upbuf, p, hh); upbuf[p++] = ':';
	p = h_ap2(upbuf, p, mm_time); upbuf[p++] = ':';
	p = h_ap2(upbuf, p, ss); upbuf[p] = 0;

	uart_printf(CONSOLE, "\r\n");
	uart_printf(CONSOLE, "  %s%s%s@%s%s%s\r\n", Y, PROJECT_USER, R, C, PROJECT_HOSTNAME, R);
	uart_printf(CONSOLE, "  -------------------------------------------------------------------------------------------\r\n");
	uart_printf(CONSOLE, "%s                                   \"--.%s\r\n", C, R);
	uart_printf(CONSOLE, "%s    ____====\"____--------------_____//______    _________________________%s\r\n", C, R);
	uart_printf(CONSOLE, "%s   //! ||   ==== ==== ==== ==== ====   || !\\\\   | _  _   ___   ___  ___   _%s\r\n", C, R);
	uart_printf(CONSOLE, "%s  (____||___====_====_====_====_====___||____) |||_||_| |___| |___||___| |_%s\r\n", C, R);
	uart_printf(CONSOLE, "%s.._____________________DB_____________________.||| | ___DB________________%s\r\n", C, R);
	uart_printf(CONSOLE, "%s   //( )--( )--( ) +--------+ ( )--( )--( )\\\\   ----'( )---( )`-------------%s\r\n", C, R);
	uart_printf(CONSOLE, "%s============================================================================%s\r\n", C, R);
	uart_printf(CONSOLE, "%s                                                   -Ken Kobayashi-%s\r\n", C, R);
	uart_printf(CONSOLE, "\r\n");
	uart_printf(CONSOLE, "  %sOS%s     : %s (CS452)\r\n", Y, R, PROJECT_DISPLAY_NAME);
	uart_printf(CONSOLE, "  %sKernel%s : AArch64 microkernel\r\n", Y, R);
	uart_printf(CONSOLE, "  %sArch%s   : Cortex-A72 (ARMv8)\r\n", Y, R);
	uart_printf(CONSOLE, "  %sCPUs%s   : 1\r\n", Y, R);
	uart_printf(CONSOLE, "  %sUptime%s : %s\r\n", Y, R, upbuf);
	uart_printf(CONSOLE, "  %sMemory%s : %u/%u MB used\r\n", Y, R, used_mb, total_mb);
	uart_printf(CONSOLE, "  %sHeap%s   : %u/%u B used\r\n", Y, R, htot - hfree, htot);
	uart_printf(CONSOLE, "  %sIPC%s    : Send / Receive / Reply\r\n", Y, R);
	uart_printf(CONSOLE, "  %sShell%s  : nix-sh  (type 'help')\r\n", Y, R);
	uart_printf(CONSOLE, "\r\n");
}

/* ===================== htop (system monitor) ===================== */

#define HTOP_LINE_MAX  256
#define HTOP_BODY_MAX  22

static void h_format_row(char *b, int width, int tid, const char *name, int pri,
                         int running, unsigned upt, int apu_style)
{
	int slot = width / 8;
	unsigned sec = upt / 100, mm = (sec / 60) % 60, ss = sec % 60, cc = upt % 100;
	int p;

	if (tid < 0) { b[0] = 0; return; }
	p = h_apu(b, 0, (unsigned)tid);
	p = h_pad(b, p, slot);     p = h_app(b, p, apu_style ? "system" : PROJECT_USER);
	p = h_pad(b, p, slot * 2); p = h_apu(b, p, (unsigned)pri);
	p = h_pad(b, p, slot * 3);
	if (apu_style) p = h_app(b, p, running ? "B" : "I");
	else           p = h_app(b, p, running ? "R" : "S");
	p = h_pad(b, p, slot * 4);
	if (apu_style) p = h_app(b, p, running ? "100.0" : "0.0");
	else           p = h_app(b, p, running ? "6.0" : "0.0");
	p = h_pad(b, p, slot * 5); p = h_app(b, p, apu_style ? "0.0" : "0.1");
	p = h_pad(b, p, slot * 6);
	if (apu_style) { p = h_apu(b, p, upt); b[p++] = 'j'; }
	else { p = h_apu(b, p, mm); b[p++] = ':'; p = h_ap2(b, p, ss); b[p++] = '.'; p = h_ap2(b, p, cc); }
	b[p] = 0;
	p = h_pad(b, p, slot * 7); p = h_app(b, p, name);
	while (p < width - 1) b[p++] = ' ';
	b[p] = 0;
}

static void h_format_header(char *hd, int width)
{
	int slot = width / 8, hp;
	hp = h_app(hd, 0, "PID");
	hp = h_pad(hd, hp, slot);     hp = h_app(hd, hp, "USER");
	hp = h_pad(hd, hp, slot * 2); hp = h_app(hd, hp, "PRI");
	hp = h_pad(hd, hp, slot * 3); hp = h_app(hd, hp, "S");
	hp = h_pad(hd, hp, slot * 4); hp = h_app(hd, hp, "CPU%");
	hp = h_pad(hd, hp, slot * 5); hp = h_app(hd, hp, "MEM%");
	hp = h_pad(hd, hp, slot * 6); hp = h_app(hd, hp, "TIME+");
	hp = h_pad(hd, hp, slot * 7); hp = h_app(hd, hp, "Command");
	while (hp < width - 1) hd[hp++] = ' ';
	hd[hp] = 0;
}

static void h_format_footer(char *ft, int width)
{
	static const char *fk[10][2] = {
		{"F1","Help"},{"F2","Setup"},{"F3","Search"},{"F4","Filter"},{"F5","List"},
		{"F6","SortBy"},{"F7","Nice-"},{"F8","Nice+"},{"F9","Kill"},{"F10","Quit"}
	};
	int p = 0;
	for (int i = 0; i < 10 && p < width - 1; i++) {
		for (const char *s = fk[i][0]; *s && p < width - 1; s++) ft[p++] = *s;
		if (p < width - 1) ft[p++] = ' ';
		for (const char *s = fk[i][1]; *s && p < width - 1; s++) ft[p++] = *s;
		if (p < width - 1) ft[p++] = ' ';
	}
	while (p < width - 1) ft[p++] = ' ';
	ft[p] = 0;
}

static void htop_build_screen(UiElem *screen, UiElem *body, char line_bufs[][HTOP_LINE_MAX],
                              int width, int rows, unsigned upt)
{
	static const char *apu_names[3] = {
		"CPU1 [Processing Unit]",
		"CPU2 [Processing Unit]",
		"CPU3 [Processing Unit]",
	};
	int n = 0, bi = 0;
	unsigned sec = upt / 100;
	int proc_rows = 0;

	body[n++] = (UiElem){ .type = UI_HTOP_METERS };

	h_app(line_bufs[bi], 0, " Tasks: 8, 1 running");
	body[n++] = (UiElem){ .type = UI_TEXT, .text = line_bufs[bi++] };
	h_app(line_bufs[bi], 0, " Load average: 0.05 0.03 0.01");
	body[n++] = (UiElem){ .type = UI_TEXT, .text = line_bufs[bi++] };
	{
		int p = h_app(line_bufs[bi], 0, " Uptime: ");
		p = h_apu(line_bufs[bi], p, sec / 3600);
		line_bufs[bi][p++] = ':';
		p = h_ap2(line_bufs[bi], p, (sec / 60) % 60);
		line_bufs[bi][p++] = ':';
		p = h_ap2(line_bufs[bi], p, sec % 60);
		line_bufs[bi][p] = 0;
	}
	body[n++] = (UiElem){ .type = UI_TEXT, .text = line_bufs[bi++] };
	body[n++] = (UiElem){ .type = UI_TEXT, .text = "" };

	h_format_header(line_bufs[bi], width);
	body[n++] = (UiElem){ .type = UI_TEXT, .text = line_bufs[bi++],
	                      .style = { .fg = 30, .bg = 42 } };

	{
		struct { int tid; const char *name; int pri; int run; } procs[] = {
			{ NameServerTid(),    "nameserver",    NAME_SERVER_PRIORITY,    0 },
			{ DisplayServerTid(), "displayserver", DISPLAY_SERVER_PRIORITY, 0 },
			{ ClockServerTid(),   "clockserver",   CLOCK_SERVER_PRIORITY,   0 },
			{ RpsServerTid(),     "rpsserver",     RPS_SERVER_PRIORITY,     0 },
#if ENABLE_LINK == 1
			{ LinkServerTid(),    "linkserver",    LINK_SERVER_PRIORITY,    0 },
#endif
#if ENABLE_MARKLIN == 1
			{ MarklinkServerTid(), "marklin",      MARKLIN_SERVER_PRIORITY, 0 },
#endif
			{ MyTid(),            "nix-sh (htop)", 20,                      1 },
		};
		for (unsigned i = 0; i < sizeof(procs) / sizeof(procs[0]); i++) {
			h_format_row(line_bufs[bi], width, procs[i].tid, procs[i].name,
			             procs[i].pri, procs[i].run, upt, 0);
			body[n++] = (UiElem){ .type = UI_TEXT, .text = line_bufs[bi++],
			                      .style = { .fg = 32, .flags = procs[i].run ? UI_BOLD : 0 } };
			proc_rows++;
		}
	}
	for (int i = 1; i <= 3; i++) {
		int busy = accel_is_busy(i);
		h_format_row(line_bufs[bi], width, i, apu_names[i - 1], 0, busy,
		             accel_jobs_done(i), 1);
		body[n++] = (UiElem){ .type = UI_TEXT, .text = line_bufs[bi++],
		                      .style = { .fg = 36, .flags = UI_BOLD } };
		proc_rows++;
	}

	body[n++] = (UiElem){ .type = UI_TEXT, .text = "" };

	{
		int used = UI_SCREEN_METER_ROWS + 1 + 3 + 1 + 1 + proc_rows + 1 + 1;
		int spacer = rows - used;
		if (spacer > 0)
			body[n++] = (UiElem){ .type = UI_SPACER, .h = spacer };
	}

	h_format_footer(line_bufs[bi], width);
	body[n++] = (UiElem){ .type = UI_TEXT, .text = line_bufs[bi++],
	                      .style = { .fg = 30, .bg = 46 } };

	*screen = (UiElem){ .type = UI_SCREEN, .children = body, .n_children = n };
}

/* read one logical key; parse ESC sequences for F1-F10; returns -1 if none ready.
 * F1-F10 are returned as 0x101-0x10A to avoid collision with printable chars. */
static int h_readkey(void)
{
	if (!uart_rxc(CONSOLE)) return -1;
	int k = (unsigned char)uart_getc(CONSOLE);
	if (k != '\033') return k;
	/* ESC: try to parse [ nn ~ or O P/Q/R/S sequences */
	if (!uart_rxc(CONSOLE)) return '\033';
	int k2 = (unsigned char)uart_getc(CONSOLE);
	if (k2 == '[') {
		if (!uart_rxc(CONSOLE)) return '\033';
		int k3 = (unsigned char)uart_getc(CONSOLE);
		if (k3 >= '1' && k3 <= '2') {
			if (!uart_rxc(CONSOLE)) return '\033';
			int k4 = (unsigned char)uart_getc(CONSOLE);
			if (uart_rxc(CONSOLE)) uart_getc(CONSOLE); /* consume trailing ~ */
			if (k3 == '1' && k4 == '1') return 0x101; /* F1  \033[11~ */
			if (k3 == '1' && k4 == '2') return 0x102; /* F2  \033[12~ */
			if (k3 == '1' && k4 == '3') return 0x103; /* F3  \033[13~ */
			if (k3 == '1' && k4 == '4') return 0x104; /* F4  \033[14~ */
			if (k3 == '1' && k4 == '5') return 0x105; /* F5  \033[15~ */
			if (k3 == '1' && k4 == '7') return 0x106; /* F6  \033[17~ */
			if (k3 == '1' && k4 == '8') return 0x107; /* F7  \033[18~ */
			if (k3 == '1' && k4 == '9') return 0x108; /* F8  \033[19~ */
			if (k3 == '2' && k4 == '0') return 0x109; /* F9  \033[20~ */
			if (k3 == '2' && k4 == '1') return 0x10A; /* F10 \033[21~ */
		}
	} else if (k2 == 'O') {
		if (!uart_rxc(CONSOLE)) return '\033';
		int k3 = (unsigned char)uart_getc(CONSOLE);
		if (k3 == 'P') return 0x101; /* F1 \033OP */
		if (k3 == 'Q') return 0x102; /* F2 \033OQ */
		if (k3 == 'R') return 0x103; /* F3 \033OR */
		if (k3 == 'S') return 0x104; /* F4 \033OS */
	}
	return '\033';
}

/* F9 Kill dialog: prompt for a TID and call Kill() */
static void h_kill_dialog(int sr, int screen_width)
{
	(void)screen_width;
	char buf[8]; int p = 0;
	uart_printf(CONSOLE, "\033[%d;1H\033[K\033[1;37;41m Kill TID: \033[0m", sr - 1);
	uart_printf(CONSOLE, UI_SHOW_CUR);
	for (;;) {
		int k = (unsigned char)uart_getc(CONSOLE);
		if (k == '\r' || k == '\n') break;
		if (k == '\033' || k == 3) { p = 0; break; }
		if ((k == 127 || k == '\b') && p > 0) { p--; uart_printf(CONSOLE, "\b \b"); continue; }
		if (k >= '0' && k <= '9' && p < 7) { buf[p++] = (char)k; uart_printf(CONSOLE, "%c", k); }
	}
	uart_printf(CONSOLE, UI_HIDE_CUR);
	buf[p] = 0;
	if (p > 0) {
		int tid = 0;
		for (int i = 0; i < p; i++) tid = tid * 10 + (buf[i] - '0');
		Kill(tid);
	}
	/* clear the dialog row */
	uart_printf(CONSOLE, "\033[%d;1H\033[K", sr - 1);
}

static void cmd_htop(void)
{
	int clock = ClockServerTid();
	int first = 1;
	UiElem body[HTOP_BODY_MAX];
	char lines[HTOP_BODY_MAX][HTOP_LINE_MAX];
	UiElem screen;

	display_clear();
	display_cursor_hide();
	for (;;) {
		int rows, cols;
		display_get_size(&rows, &cols);
		unsigned upt = (unsigned)(clock >= 0 ? Time(clock) : 0);

		htop_build_screen(&screen, body, lines, cols, rows, upt);

		if (first) { display_render_tree_full(&screen); first = 0; }
		else       { display_render_tree(&screen); }

		int k = h_readkey();
		if (k < 0) {
			/* no key */
		} else if (k == 'q' || k == 'Q' || k == '0' || k == '\033' || k == 0x10A) {
			break;
		} else if (k == '9' || k == 0x109) {
			h_kill_dialog(rows, cols);
			first = 1;
		}

		if (clock >= 0) Delay(clock, 7);
	}
	display_clear();
	display_cursor_show();
}

#if ENABLE_LINK == 1
/* Talk to the virtual hardware on the docker container over the AUX mini-UART
 * (serial1). `link <cmd...>` sends one line and prints the device's reply;
 * `link` alone asks the device for its HELP. This is the OS reaching the
 * outside world: the container runs tools/vhw.py (a Marklin sim + gateway). */
static void cmd_link(const char *args)
{
	char reply[LINK_REPLY_MAX + 1];
	const char *cmd = (args && args[0]) ? args : "HELP";
	LinkSend(cmd, reply, (int)sizeof(reply));
	uart_printf(CONSOLE, "  %s\r\n", reply);
}
#endif /* ENABLE_LINK (cmd_link) */

/* Set / show the screen resolution. The screen is a character grid (cols x rows),
 * so "resolution" here is the terminal size and "ratio" is a device-shaped preset
 * (cell ~2:1, so a preset's cols/rows approximates the device's pixel aspect). */
static int sc_word_eq(const char *a, const char *b)
{
	int i = 0; while (a[i] && b[i] && a[i] == b[i]) i++;
	return a[i] == 0 && (b[i] == 0 || b[i] == ' ');
}
static const char *sc_skip_num(const char *s, int *out)
{
	int v = 0, got = 0;
	while (*s == ' ') s++;
	while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; got = 1; }
	*out = got ? v : -1;
	return s;
}
static void cmd_screen(const char *args)
{
	static const struct { const char *name; int cols, rows; } presets[] = {
		{ "desktop",   UI_DESIGN_SCREEN_COLS, UI_DESIGN_SCREEN_ROWS },
		{ "laptop",     80, 50 },  /* 16:10 */
		{ "ultrawide", 116, 24 },  /* 21:9  */
		{ "superwide", 116, 18 },  /* 32:9  */
		{ "tablet",     72, 28 },  /* 4:3   */
		{ "phone",      40, 44 },  /* portrait ~9:20 */
		{ "tv",        116, 34 },  /* 16:9 big */
	};
	if (!args[0]) {
		uart_printf(CONSOLE, "  screen: %d cols x %d rows (%s)\r\n",
			    screen_cols(), screen_rows(), screen_is_manual() ? "manual" : "auto");
		uart_printf(CONSOLE, "  usage: screen auto | <cols> <rows> | <preset>\r\n");
		uart_printf(CONSOLE, "  presets: desktop laptop ultrawide superwide tablet phone tv\r\n");
		return;
	}
	if (sc_word_eq("auto", args)) {
		screen_auto();
		screen_query();
		uart_printf(CONSOLE, "  screen: autofit -> %d x %d\r\n", screen_cols(), screen_rows());
		return;
	}
	for (unsigned i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
		if (sc_word_eq(presets[i].name, args)) {
			screen_set(presets[i].cols, presets[i].rows);
			uart_printf(CONSOLE, "  screen: %s -> %d x %d (manual)\r\n",
				    presets[i].name, screen_cols(), screen_rows());
			return;
		}
	}
	int cols = -1, rows = -1;
	const char *p = sc_skip_num(args, &cols);
	sc_skip_num(p, &rows);
	if (cols > 0 && rows > 0) {
		screen_set(cols, rows);
		uart_printf(CONSOLE, "  screen: %d x %d (manual)\r\n", screen_cols(), screen_rows());
		return;
	}
	uart_printf(CONSOLE, "  usage: screen auto | <cols> <rows> | <preset>\r\n");
}

#if ENABLE_LINK == 1
/* libc-free helpers for the self-test */
static int lt_eq(const char *a, const char *b)
{
	int i = 0; while (a[i] && a[i] == b[i]) i++; return a[i] == b[i];
}
static int lt_sub(const char *h, const char *n)   /* n is a substring of h? */
{
	for (int i = 0; h[i]; i++) {
		int j = 0; while (n[j] && h[i + j] == n[j]) j++;
		if (!n[j]) return 1;
	}
	return 0;
}

/* Handshake + round-trip self-test: prove the OS is wired to the RIGHT virtual
 * hardware (the Marklin mock), not just that "something" answered. Checks the
 * device identity, a liveness ping, a per-run echo nonce (no stale/dup link),
 * and a compute (the device actually processes, doesn't just echo). */
static void cmd_linktest(void)
{
	char r[LINK_REPLY_MAX + 1];
	char cmd[64], exp[64];
	int pass = 0, total = 0, p, ok;

	/* 1. identity handshake */
	total++;
	LinkSend("ID", r, (int)sizeof(r));
	ok = lt_sub(r, "MARKLIN-MOCK") && lt_sub(r, "d273liu-nix-vhw");
	pass += ok;
	uart_printf(CONSOLE, "  [%s] identity   ID -> %s\r\n", ok ? "PASS" : "FAIL", r);

	/* 2. liveness ping */
	total++;
	LinkSend("PING", r, (int)sizeof(r));
	ok = lt_eq(r, "PONG");
	pass += ok;
	uart_printf(CONSOLE, "  [%s] liveness   PING -> %s\r\n", ok ? "PASS" : "FAIL", r);

	/* 3. echo nonce (fresh each run -> not a stale buffer or wrong peer) */
	total++;
	{
		unsigned nonce = ui_rng() % 100000u;
		p = 0; app_append(cmd, &p, "ECHO NIX-"); p += app_uitoa(nonce, cmd + p); cmd[p] = 0;
		p = 0; app_append(exp, &p, "NIX-");      p += app_uitoa(nonce, exp + p); exp[p] = 0;
	}
	LinkSend(cmd, r, (int)sizeof(r));
	ok = lt_eq(r, exp);
	pass += ok;
	uart_printf(CONSOLE, "  [%s] echo       %s -> %s\r\n", ok ? "PASS" : "FAIL", cmd, r);

	/* 4. compute (device processes, not echoes): 19 + 23 = 42 */
	total++;
	LinkSend("ADD 19 23", r, (int)sizeof(r));
	ok = lt_eq(r, "42");
	pass += ok;
	uart_printf(CONSOLE, "  [%s] compute    ADD 19 23 -> %s\r\n", ok ? "PASS" : "FAIL", r);

	uart_printf(CONSOLE, "  link self-test: %d/%d passed -- %s\r\n",
		    pass, total,
		    pass == total ? "CONNECTED TO MARKLIN MOCK" : "LINK MISMATCH / NO DEVICE");
}
#endif /* ENABLE_LINK */

#if ENABLE_MARKLIN == 1
static int mk_atoi(const char *s)
{
	int n = 0, neg = 0;
	while (*s == ' ') s++;
	if (*s == '-') { neg = 1; s++; }
	while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
	return neg ? -n : n;
}

static void cmd_tr(const char *args)
{
	int addr = -1, speed = -1;
	const char *p = args;
	while (*p == ' ') p++;
	addr = mk_atoi(p);
	while (*p && *p != ' ') p++;
	while (*p == ' ') p++;
	speed = mk_atoi(p);
	if (addr <= 0 || speed < 0 || speed > 15) {
		uart_printf(CONSOLE, "  usage: tr <addr 1-80> <speed 0-15>\r\n");
		return;
	}
	MarklinkSetSpeed(addr, speed);
	uart_printf(CONSOLE, "  train %d speed %d\r\n", addr, speed);
}

static void cmd_sw(const char *args)
{
	int addr = -1, dir = -1;
	const char *p = args;
	while (*p == ' ') p++;
	addr = mk_atoi(p);
	while (*p && *p != ' ') p++;
	while (*p == ' ') p++;
	if (*p == 'S' || *p == 's') dir = MARKLIN_SW_STRAIGHT;
	else if (*p == 'C' || *p == 'c') dir = MARKLIN_SW_CURVED;
	if (addr <= 0 || dir < 0) {
		uart_printf(CONSOLE, "  usage: sw <addr> <S|C>\r\n");
		return;
	}
	MarklinkSetSwitch(addr, dir);
	uart_printf(CONSOLE, "  switch %d -> %s\r\n", addr,
		    dir == MARKLIN_SW_STRAIGHT ? "straight" : "curved");
}

static void cmd_sensors(void)
{
	MarklinkSensors s = MarklinkPollSensors();
	int any = 0;
	uart_printf(CONSOLE, "  Sensors (A-E, 1-16):\r\n");
	for (int mod = 1; mod <= MARKLIN_MODULES; mod++) {
		char name[4];
		int printed = 0;
		for (int sens = 1; sens <= MARKLIN_SENS_PER_M; sens++) {
			if (marklin_sens_get(&s, mod, sens)) {
				marklin_sens_name(mod, sens, name);
				if (!printed) {
					uart_printf(CONSOLE, "    %c: ", 'A' + mod - 1);
					printed = 1;
				}
				uart_printf(CONSOLE, "%s ", name);
				any = 1;
			}
		}
		if (printed) uart_printf(CONSOLE, "\r\n");
	}
	if (!any) uart_printf(CONSOLE, "  (no sensors triggered)\r\n");
}

/* ---- ASCII track map ------------------------------------------------- */

/*
 * Simplified schematic of CS452 Track A.
 * Sensors are shown as two-char labels; the train position (from TC1)
 * is highlighted in yellow.  Switches are shown as junction chars.
 *
 * Physical layout (approximate, 78 cols):
 *
 *   EX5 EX8 EX4 EX6
 *    |   |   |   |                     (dead-end stubs at top)
 *  A1-A2-A3-A4-A5-A6-A7-A8-A9-A10-A11-A12-A13-A14-A15-A16  (top straight)
 *   \ /   |       |       |         |         |    \   /
 *  BR3   BR14   MR1/2/3  MR4      BR1-2-3    BR4  (crossovers)
 *   / \   |       |       |         |         |    /   \
 *  B1-B2-B3-B4-B5-B6-B7-B8-B9-B10-B11-B12-B13-B14-B15-B16  (bottom straight)
 *                 |                       |
 *                C1-C2-...-C16           D1-...-D16
 *                           |                   |
 *                       E sensors (crossover area)
 */
static void cmd_track_map(const char *arg)
{
	(void)arg;
#if ENABLE_MARKLIN
	MarklinkSensors s = MarklinkPollSensors();
#define SL "\033[1;33m"   /* yellow highlight (active sensor) */
#define SR "\033[0m"       /* reset */
#define SC(mod,n) (marklin_sens_get(&s,(mod),(n)) ? SL : "")
#define SE(mod,n) (marklin_sens_get(&s,(mod),(n)) ? SR : "")
#else
#define SC(mod,n) ""
#define SE(mod,n) ""
#endif
	uart_printf(CONSOLE, "\r\n");
	uart_printf(CONSOLE,
	    "\033[1;36m━━━ CS452 Track A ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\033[0m\r\n");
	/* Row 1: dead-end exits along the top */
	uart_printf(CONSOLE,
	    "  EX5  EX8  EX4 EX6     EX3          EX1  EX2\r\n");
	uart_printf(CONSOLE,
	    "   |    |    |   |       |             |    |\r\n");
	/* Row 2: A sensors (top straight) */
	uart_printf(CONSOLE,
	    " %sA1%s-%sA2%s-%sA3%s-%sA4%s-%sA5%s-%sA6%s-%sA7%s-%sA8%s-%sA9%s-"
	    "%sA10%s-%sA11%s-%sA12%s-%sA13%s-%sA14%s-%sA15%s-%sA16%s\r\n",
	    SC(1,1),SE(1,1), SC(1,2),SE(1,2), SC(1,3),SE(1,3), SC(1,4),SE(1,4),
	    SC(1,5),SE(1,5), SC(1,6),SE(1,6), SC(1,7),SE(1,7), SC(1,8),SE(1,8),
	    SC(1,9),SE(1,9), SC(1,10),SE(1,10), SC(1,11),SE(1,11), SC(1,12),SE(1,12),
	    SC(1,13),SE(1,13), SC(1,14),SE(1,14), SC(1,15),SE(1,15), SC(1,16),SE(1,16));
	/* Row 3: junction connectors */
	uart_printf(CONSOLE,
	    "  |  \\ BR3   \\BR14 MR1--MR2--MR3     MR4      BR4   |\r\n");
	uart_printf(CONSOLE,
	    "  |   \\ /     |    |                  |        / \\   |\r\n");
	/* Row 4: B sensors (bottom of main oval — reversed direction) */
	uart_printf(CONSOLE,
	    " %sB1%s-%sB2%s-%sB3%s-%sB4%s-%sB5%s-%sB6%s-%sB7%s-%sB8%s-%sB9%s-"
	    "%sB10%s-%sB11%s-%sB12%s-%sB13%s-%sB14%s-%sB15%s-%sB16%s\r\n",
	    SC(2,1),SE(2,1), SC(2,2),SE(2,2), SC(2,3),SE(2,3), SC(2,4),SE(2,4),
	    SC(2,5),SE(2,5), SC(2,6),SE(2,6), SC(2,7),SE(2,7), SC(2,8),SE(2,8),
	    SC(2,9),SE(2,9), SC(2,10),SE(2,10), SC(2,11),SE(2,11), SC(2,12),SE(2,12),
	    SC(2,13),SE(2,13), SC(2,14),SE(2,14), SC(2,15),SE(2,15), SC(2,16),SE(2,16));
	/* Row 5: branches to inner tracks */
	uart_printf(CONSOLE,
	    "           |             |                  |          |\r\n");
	uart_printf(CONSOLE,
	    "        BR16             D14                D16    BR15 (->C5/C10)\r\n");
	/* Row 6: C sensors (inner track) */
	uart_printf(CONSOLE,
	    " %sC1%s-%sC2%s-%sC3%s-%sC4%s-%sC5%s-%sC6%s-%sC7%s-%sC8%s-%sC9%s-"
	    "%sC10%s-%sC11%s-%sC12%s-%sC13%s-%sC14%s-%sC15%s-%sC16%s\r\n",
	    SC(3,1),SE(3,1), SC(3,2),SE(3,2), SC(3,3),SE(3,3), SC(3,4),SE(3,4),
	    SC(3,5),SE(3,5), SC(3,6),SE(3,6), SC(3,7),SE(3,7), SC(3,8),SE(3,8),
	    SC(3,9),SE(3,9), SC(3,10),SE(3,10), SC(3,11),SE(3,11), SC(3,12),SE(3,12),
	    SC(3,13),SE(3,13), SC(3,14),SE(3,14), SC(3,15),SE(3,15), SC(3,16),SE(3,16));
	uart_printf(CONSOLE,
	    "  |   MR153 EX3  BR6    BR16 BR13  MR14     BR11        |\r\n");
	/* Row 7: D sensors */
	uart_printf(CONSOLE,
	    " %sD1%s-%sD2%s-%sD3%s-%sD4%s-%sD5%s-%sD6%s-%sD7%s-%sD8%s-%sD9%s-"
	    "%sD10%s-%sD11%s-%sD12%s-%sD13%s-%sD14%s-%sD15%s-%sD16%s\r\n",
	    SC(4,1),SE(4,1), SC(4,2),SE(4,2), SC(4,3),SE(4,3), SC(4,4),SE(4,4),
	    SC(4,5),SE(4,5), SC(4,6),SE(4,6), SC(4,7),SE(4,7), SC(4,8),SE(4,8),
	    SC(4,9),SE(4,9), SC(4,10),SE(4,10), SC(4,11),SE(4,11), SC(4,12),SE(4,12),
	    SC(4,13),SE(4,13), SC(4,14),SE(4,14), SC(4,15),SE(4,15), SC(4,16),SE(4,16));
	uart_printf(CONSOLE,
	    "  | BR155 MR10 MR9       BR8  BR9  MR7  MR8  BR17  MR17 |\r\n");
	/* Row 8: E sensors (crossover/middle area) */
	uart_printf(CONSOLE,
	    " %sE1%s-%sE2%s-%sE3%s-%sE4%s-%sE5%s-%sE6%s-%sE7%s-%sE8%s-%sE9%s-"
	    "%sE10%s-%sE11%s-%sE12%s-%sE13%s-%sE14%s-%sE15%s-%sE16%s\r\n",
	    SC(5,1),SE(5,1), SC(5,2),SE(5,2), SC(5,3),SE(5,3), SC(5,4),SE(5,4),
	    SC(5,5),SE(5,5), SC(5,6),SE(5,6), SC(5,7),SE(5,7), SC(5,8),SE(5,8),
	    SC(5,9),SE(5,9), SC(5,10),SE(5,10), SC(5,11),SE(5,11), SC(5,12),SE(5,12),
	    SC(5,13),SE(5,13), SC(5,14),SE(5,14), SC(5,15),SE(5,15), SC(5,16),SE(5,16));
	uart_printf(CONSOLE,
	    "  BR156  BR10  BR7  MR8  BR13  MR13  MR6  BR5  MR5  MR156\r\n");
	uart_printf(CONSOLE,
	    "\033[1;36m━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\033[0m\r\n");
	uart_printf(CONSOLE,
	    "  \033[1;33m█\033[0m=active sensor  -=track  BR=switch  MR=merge  EX=dead end\r\n");
	uart_printf(CONSOLE, "\r\n");
#undef SL
#undef SR
#undef SC
#undef SE
}
#endif /* ENABLE_MARKLIN */

void shell(void)
{
	char line[SHELL_LINE_MAX];

	ramfs_init();
	shell_frame_draw("Type 'help'");
	cmd_neofetch();
	shell_write("\033[1;36m[ shell ]\033[0m ");
	shell_write(PROJECT_DISPLAY_NAME);
	shell_write(" terminal. Type 'help'.\r\n");

	for (;;) {
		shell_prompt_print(cwd);
		display_readline_start();
		shell_readline(line, SHELL_LINE_MAX, cwd);

		/* Command is a string; size 0 → no command. */
		if (line[0] == '\0')
			continue;

		char *args = split_args(line);

		if (strcmp_ret(line, "help")) {
			cmd_help();
		} else if (strcmp_ret(line, "echo")) {
			uart_printf(CONSOLE, "%s\r\n", args);
		} else if (strcmp_ret(line, "mem")) {
			cmd_mem();
		} else if (strcmp_ret(line, "pages")) {
			uint64_t total = pmm_total_pages();
			uint64_t freep = pmm_free_pages_count();
			uart_printf(CONSOLE,
			    "frames: total=%u  used=%u  free=%u  (page=%u B)\r\n",
			    (unsigned)total, (unsigned)(total - freep), (unsigned)freep,
			    (unsigned)PAGE_SIZE);
		} else if (strcmp_ret(line, "run")) {
			uart_printf(CONSOLE, "launching demo C program (own EL0 task)...\r\n");
			int t = Create(10, demo_program);   /* higher precedence: runs now */
			uart_printf(CONSOLE, "created task tid=%d\r\n", t);
			for (int i = 0; i < 30; i++) Yield();  /* let it run to completion */
		} else if (strcmp_ret(line, "uptime")) {
			uart_printf(CONSOLE, "uptime: %u ticks\r\n", (unsigned)GetKernelRuntime());
		} else if (strcmp_ret(line, "tid")) {
			uart_printf(CONSOLE, "tid: %d\r\n", MyTid());
		} else if (strcmp_ret(line, "clear")) {
			shell_frame_draw("Type 'help'");
		} else if (strcmp_ret(line, "rps")) {
			app_rps();
		} else if (strcmp_ret(line, "snake")) {
			app_snake();
		} else if (strcmp_ret(line, "tetris")) {
			app_tetris();
		} else if (strcmp_ret(line, "ls")) {
			cmd_ls(args);
		} else if (strcmp_ret(line, "cd")) {
			cmd_cd(args);
		} else if (strcmp_ret(line, "pwd")) {
			uart_printf(CONSOLE, "%s\r\n", cwd);
		} else if (strcmp_ret(line, "mkdir")) {
			cmd_mkdir(args);
		} else if (strcmp_ret(line, "cat")) {
			cmd_cat(args);
		} else if (strcmp_ret(line, "write")) {
			cmd_write(args);
		} else if (strcmp_ret(line, "nano")) {
			cmd_nano(args);
		} else if (strcmp_ret(line, "rm")) {
			cmd_rm(args);
		} else if (strcmp_ret(line, "tree")) {
			cmd_tree();
		} else if (strcmp_ret(line, "neofetch") || strcmp_ret(line, "splash")) {
			cmd_neofetch();
		} else if (strcmp_ret(line, "htop")) {
			cmd_htop();
		} else if (strcmp_ret(line, "screen")) {
			cmd_screen(args);
#if ENABLE_LINK == 1
		} else if (strcmp_ret(line, "link")) {
			cmd_link(args);
		} else if (strcmp_ret(line, "linktest")) {
			cmd_linktest();
#endif
#if ENABLE_MARKLIN == 1
		} else if (strcmp_ret(line, "tr")) {
			cmd_tr(args);
		} else if (strcmp_ret(line, "sw")) {
			cmd_sw(args);
		} else if (strcmp_ret(line, "sensors")) {
			cmd_sensors();
		} else if (strcmp_ret(line, "tc1")) {
			char *sub = split_args(args);   /* sub = args after first word */
			if (strcmp_ret(args, "start")) {
				if (TC1Tid() >= 0) {
					uart_printf(CONSOLE, "tc1: already running (tid %d)\r\n", TC1Tid());
				} else {
					int tid = Create(TC1_PRIORITY, tc1_entry);
					uart_printf(CONSOLE, "tc1: started (tid %d)\r\n", tid);
				}
			} else if (strcmp_ret(args, "goto")) {
				if (!sub || !sub[0]) {
					uart_printf(CONSOLE, "usage: tc1 goto <sensor>  e.g. tc1 goto A5\r\n");
				} else if (TC1Tid() < 0) {
					uart_printf(CONSOLE, "tc1: not running (use 'tc1 start' first)\r\n");
				} else {
					TC1Goto(sub);
				}
			} else if (strcmp_ret(args, "speed")) {
				if (!sub || !sub[0]) {
					uart_printf(CONSOLE, "usage: tc1 speed <0-14>\r\n");
				} else if (TC1Tid() < 0) {
					uart_printf(CONSOLE, "tc1: not running\r\n");
				} else {
					int sp = 0;
					for (int i = 0; sub[i] >= '0' && sub[i] <= '9'; i++)
						sp = sp * 10 + (sub[i] - '0');
					if (sp < 0) sp = 0;
					if (sp > 14) sp = 14;
					TC1Speed(sp);
				}
			} else if (strcmp_ret(args, "stop")) {
				if (TC1Tid() < 0) {
					uart_printf(CONSOLE, "tc1: not running\r\n");
				} else {
					TC1Speed(0);
				}
			} else {
				uart_printf(CONSOLE,
					"usage: tc1 start|stop|goto <sensor>|speed <0-14>\r\n"
					"  tc1 start         launch train controller\r\n"
					"  tc1 goto <sensor> route and stop at sensor (e.g. A5, B12)\r\n"
					"  tc1 speed <N>     set speed 0-14\r\n"
					"  tc1 stop          stop train immediately\r\n");
			}
#endif
		} else if (strcmp_ret(line, "about")) {
			cmd_about();
		} else if (strcmp_ret(line, "map")) {
			cmd_track_map(args);
		} else {
			uart_printf(CONSOLE, "unknown command: '%s' (try 'help')\r\n", line);
		}
	}
}

/* =================== Layer 4 UI launcher (the default UI) =================== */

static void ui_about_app(void)
{
	static const char *lines[] = {
		"AArch64 CS452 microkernel.",
		"Single CPU.",
		"Servers: Name, Clock, RPS.",
		"IPC: Send / Receive / Reply.",
		"",
	};
	UiElem texts[8];
	int n = 0;
	for (unsigned i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
		texts[n++] = (UiElem){ .type = UI_TEXT, .text = lines[i] };

	int rows, cols;
	UiAppShell sh;
	ui_app_enter();
	display_get_size(&rows, &cols);
	ui_app_shell_init(&sh, "About", "any key to go back", rows, texts, n);
	ui_app_shell_render(&sh, 1);
	(void)ui_getch();
	ui_app_leave();
}

static void ui_clock_app(void)
{
	int clock = ClockServerTid();
	char l1[80], l2[80];
	UiElem texts[2] = {
		{ .type = UI_TEXT, .text = l1 },
		{ .type = UI_TEXT, .text = l2 },
	};
	int rows, cols;
	UiAppShell sh;

	ui_app_enter();
	display_get_size(&rows, &cols);
	ui_app_shell_init(&sh, "System Clock", "any key to go back", rows, texts, 2);
	for (;;) {
		unsigned upt = (unsigned)(clock >= 0 ? Time(clock) : 0);
		int p = h_app(l1, 0, "Uptime : ");
		p = h_apu(l1, p, upt);
		p = h_app(l1, p, " ticks (10ms)");
		l1[p] = 0;
		p = h_app(l2, 0, "Runtime: ");
		p = h_apu(l2, p, (unsigned)GetKernelRuntime());
		l2[p] = 0;
		ui_app_shell_render(&sh, 0);
		if (uart_rxc(CONSOLE)) { (void)uart_getc(CONSOLE); break; }
		if (clock >= 0) Delay(clock, 10);
	}
	ui_app_leave();
}

/* Query the terminal size via the ANSI cursor-position report: move far, ask
 * with ESC[6n, read the "ESC[rows;colsR" reply. Returns 1 and sets rows/cols on
 * success, 0 if the terminal never answers (e.g. a pipe). Bounded; never hangs. */
/* After boot completes: advisory if the terminal window is too small for the grid. */
void ui_boot_advisory(void)
{
	int rows = 0, cols = 0;
	char line[96];
	int p;

	display_get_size(&rows, &cols);
	if (cols >= UI_DESIGN_SCREEN_COLS && rows >= UI_DESIGN_SCREEN_ROWS)
		return;

	display_write_str("\r\n\033[1;33m[ INFO ]\033[0m Screen size advisory\r\n");

	p = 0;
	for (const char *s = "  Screen is "; *s && p < (int)sizeof(line) - 1; s++)
		line[p++] = *s;
	p += ui_utoa((unsigned)cols, line + p, (int)sizeof(line) - p);
	for (const char *s = " cols x "; *s && p < (int)sizeof(line) - 1; s++)
		line[p++] = *s;
	p += ui_utoa((unsigned)rows, line + p, (int)sizeof(line) - p);
	for (const char *s = " rows.\r\n"; *s && p < (int)sizeof(line) - 1; s++)
		line[p++] = *s;
	line[p] = 0;
	display_write_str(line);

	p = 0;
	for (const char *s = "  Apps are sized for the default screen ("; *s && p < (int)sizeof(line) - 1; s++)
		line[p++] = *s;
	p += ui_utoa(UI_DESIGN_SCREEN_COLS, line + p, (int)sizeof(line) - p);
	for (const char *s = " x "; *s && p < (int)sizeof(line) - 1; s++)
		line[p++] = *s;
	p += ui_utoa(UI_DESIGN_SCREEN_ROWS, line + p, (int)sizeof(line) - p);
	for (const char *s = ") or larger.\r\n"; *s && p < (int)sizeof(line) - 1; s++)
		line[p++] = *s;
	line[p] = 0;
	display_write_str(line);

	display_write_str("  Widen the window or zoom out, then tap a key.\r\n");
	(void)display_wait_key();
}

static void ui_launcher(void);

void ui_boot_entry(void)
{
	display_restore_persisted_screen();
	ui_boot_advisory();
	ui_launcher();
}

typedef struct {
	const char *name;
	const char *icon;
	void      (*fn)(void);
} LauncherApp;

static void run_terminal(void)
{
	display_cursor_show();
	shell();
	display_cursor_hide();
}

static const LauncherApp g_apps[] = {
	{ "Terminal", ">_", run_terminal },
	{ "Snake",    "~~", app_snake    },
	{ "Tetris",   "##", app_tetris   },
	{ "Pong",     "O|", app_pong     },
	{ "RPS",      "RP", app_rps      },
	{ "About",    "i?", ui_about_app },
	{ "Clock",    "()", ui_clock_app },
};
#define N_APPS ((int)(sizeof(g_apps) / sizeof(g_apps[0])))

static void launcher_status(char *out, int cap, const char *sel_name,
                            int page, int npages)
{
	const char *pre = "[]/Arrows  Pg";
	int p = 0;
	for (const char *s = pre; *s && p < cap - 1; s++) out[p++] = *s;
	p = h_apu(out, p, (unsigned)page);
	if (p < cap - 1) out[p++] = '/';
	p = h_apu(out, p, (unsigned)npages);
	if (p < cap - 1) out[p++] = ' ';
	out[p++] = 's'; out[p++] = 'e'; out[p++] = 'l'; out[p++] = ':';
	if (p < cap - 1) out[p++] = ' ';
	for (const char *s = sel_name; *s && p < cap - 1; s++) out[p++] = *s;
	out[p] = 0;
}

/* Fit grid inside meter row + div chrome; never exceed screen bottom. */
static void launcher_grid_layout(int scr_cols, int scr_rows, int n_apps,
                                 int *ncols, int *tile_h, int *vrows)
{
	int inner_w = UI_DIV_INNER_W(scr_cols);
	int max_grid_h = UI_GRID_MAX_H(scr_rows);
	if (max_grid_h < UI_TILE_MIN_H) max_grid_h = UI_TILE_MIN_H;

	int cols = UI_LAUNCHER_COLS;
	if (cols > n_apps) cols = n_apps;
	while (cols > 1 && UI_SEG_W(inner_w, cols, 0) < UI_TILE_MIN_W)
		cols--;

	int seg_w = UI_SEG_W(inner_w, cols, 0);
	int max_th = seg_w / UI_TILE_ASPECT;
	if (max_th < UI_TILE_MIN_H) max_th = UI_TILE_MIN_H;

	int th = UI_TILE_MIN_H;
	if (max_th < th) th = max_th;

	int vr = max_grid_h / th;
	if (vr < 1) vr = 1;

	/* Single visible row: grow tile height, still within column + screen. */
	if (vr == 1 && max_grid_h > th) {
		th = max_grid_h < max_th ? max_grid_h : max_th;
		if (th < UI_TILE_MIN_H) th = UI_TILE_MIN_H;
	}

	/* Hard guard: grid rows must not push div past screen bottom. */
	while (vr > 1 && vr * th > max_grid_h) vr--;
	if (vr == 1 && th > max_grid_h)
		th = max_grid_h;
	if (th < UI_TILE_MIN_H) th = UI_TILE_MIN_H;

	/* Grow tiles to fill the grid band (paired with div.h edge-to-edge layout). */
	{
		int th_fill = max_grid_h / vr;
		if (th_fill > max_th)
			th_fill = max_th;
		if (th_fill >= UI_TILE_MIN_H)
			th = th_fill;
	}

	*ncols = cols;
	*tile_h = th;
	*vrows = vr;
}

static int launcher_sync_page(int sel, int ncols, int vrows, int n_apps)
{
	int npages = UI_GRID_NPAGES(n_apps, ncols, vrows);
	int page = UI_GRID_PAGE_INDEX(sel, ncols, vrows);
	if (page < 0) page = 0;
	if (page >= npages) page = npages - 1;
	return page;
}

/*
 * Layer 4 UI home screen — full-width tile grid (not a numbered list).
 * Rendered through the display server's UiElem path with live meter row.
 */
static void ui_launcher(void)
{
	int sel = 0;
	int first = 1;

	for (;;) {
		int rows, cols;
		display_get_size(&rows, &cols);

		int ncols, tile_h, vrows;
		launcher_grid_layout(cols, rows, N_APPS, &ncols, &tile_h, &vrows);
		int npages = UI_GRID_NPAGES(N_APPS, ncols, vrows);
		int page = launcher_sync_page(sel, ncols, vrows, N_APPS);
		int page_num = page + 1;

		UiElem tiles[N_APPS];
		for (int i = 0; i < N_APPS; i++)
			tiles[i] = (UiElem){ .type = UI_TILE,
			                     .title = g_apps[i].name,
			                     .text  = g_apps[i].icon };

		UiElem grid = { .type = UI_GRID, .n_cols = ncols, .sel = sel,
		                .page = page, .ch = vrows, .h = tile_h,
		                .children = tiles, .n_children = N_APPS };

		char status[80];
		launcher_status(status, (int)sizeof(status), g_apps[sel].name,
		                  page_num, npages);

		UiAppShell sh;
		UiElem content[1] = { grid };
		ui_app_shell_init(&sh, "MyNIX Apps", status, rows, content, 1);

		if (first) { display_clear(); ui_app_shell_render(&sh, 1); first = 0; }
		else       { ui_app_shell_render(&sh, 0); }

		int k = ui_getkey();

		if (k == UI_KEY_LEFT) {
			if (sel % ncols > 0) sel--;
		} else if (k == UI_KEY_RIGHT) {
			if (sel % ncols < ncols - 1 && sel + 1 < N_APPS) sel++;
		} else if (k == UI_KEY_UP) {
			if (sel - ncols >= 0) sel -= ncols;
		} else if (k == UI_KEY_DOWN) {
			if (sel + ncols < N_APPS) sel += ncols;
		} else if (k == '[') {
			page = launcher_sync_page(sel, ncols, vrows, N_APPS);
			if (page > 0) {
				sel = UI_GRID_PAGE_ROW(page - 1, vrows) * ncols;
				if (sel >= N_APPS) sel = N_APPS - 1;
			}
		} else if (k == ']') {
			page = launcher_sync_page(sel, ncols, vrows, N_APPS);
			if (page + 1 < npages) {
				sel = UI_GRID_PAGE_ROW(page + 1, vrows) * ncols;
				if (sel >= N_APPS) sel = N_APPS - 1;
			}
		} else if (k >= '1' && k < '1' + N_APPS) {
			sel = k - '1';
			display_clear();
			display_cursor_hide();
			g_apps[sel].fn();
			display_clear();
			first = 1;
		} else if (k == '\r' || k == '\n') {
			display_clear();
			display_cursor_hide();
			g_apps[sel].fn();
			display_clear();
			first = 1;
		}
	}
}

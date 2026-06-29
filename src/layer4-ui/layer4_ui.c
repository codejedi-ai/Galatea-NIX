#include "layer4_ui.h"
#include "../layer3-services/nameserver.h"
#include "../layer3-services/uart/io_api/io_api.h"
#include "../layer3-services/uart/UART1_CONSOLE_server/UART1_CONSOLE_server.h"
#include "../layer2-messaging/messaging.h"
#include "ui_app.h"
#include "ui_elem.h"
#include "rps.h"
#include "rps_messages.h"
#include "clock_client.h"
#include "clockserver.h"
#include "../layer1-processes/rpi.h"
#include "../layer1-processes/syscall.h"
#include "../layer1-processes/config.h"
#include "../layer1-processes/accel.h"
#include "../layer3-services/idle.h"
#include "../layer3-services/displayserver.h"   /* DisplayServerTid() */
#include "../layer3-services/display_client.h"  /* display_get_size()  */

/* Box-drawing (UTF-8): corners/sides for the device frame, ━ for thick rules. */
#define BX_TL "\xe2\x94\x8c"   /* ┌ */
#define BX_TR "\xe2\x94\x90"   /* ┐ */
#define BX_BL "\xe2\x94\x94"   /* └ */
#define BX_BR "\xe2\x94\x98"   /* ┘ */
#define BX_H  "\xe2\x94\x80"   /* ─ */
#define BX_V  "\xe2\x94\x82"   /* │ */
#define BX_TH "\xe2\x94\x81"   /* ━ */

/* ---- tiny local string helpers (freestanding) ---- */
static void pclear(char *b, int w) { for (int i = 0; i < w; i++) b[i] = ' '; b[w] = 0; }
static void pput(char *b, int pos, const char *s, int w)
{
	for (int i = 0; s && s[i] && pos + i < w; i++)
		b[pos + i] = s[i];
}
static int putoa(unsigned v, char *out)
{
	char t[16]; int n = 0;
	if (v == 0) t[n++] = '0';
	while (v) { t[n++] = (char)('0' + v % 10); v /= 10; }
	for (int i = 0; i < n; i++) out[i] = t[n - 1 - i];
	out[n] = 0;
	return n;
}

#define CANVAS_TOP_ROW  (UI_GAME_BARS + 1)
#define CANVAS_BODY_ROW (UI_GAME_BARS + 4)
#define CANVAS_BODY_COL 2

static void build_stats(char *out)
{
	char num[16]; int p = 0, n;
	out[p++] = 'C'; out[p++] = 'P'; out[p++] = 'U'; out[p++] = ':';
	{
		unsigned pct = cpu_util_pct();
		n = putoa(pct, num);
		for (int i = 0; i < n; i++) out[p++] = num[i];
		out[p++] = '%';
	}
	out[p++] = ' ';
	out[p++] = 'A'; out[p++] = 'P'; out[p++] = 'U'; out[p++] = ':';
	unsigned total_jobs = accel_jobs_done(1) + accel_jobs_done(2) + accel_jobs_done(3);
	n = putoa(total_jobs, num);
	for (int i = 0; i < n; i++) out[p++] = num[i];
	out[p++] = 'j';
	out[p] = 0;
}

/* write decimal v at column pos; return its width */
static int pput_num(char *b, int pos, unsigned v, char *scratch)
{
	int n = putoa(v, scratch);
	pput(b, pos, scratch, UI_W);
	return n;
}

static int h_app(char *b, int p, const char *s)
{
	for (int i = 0; s[i]; i++) b[p++] = s[i];
	b[p] = 0;
	return p;
}

static int h_apu(char *b, int p, unsigned v)
{
	char t[16];
	int n = putoa(v, t);
	for (int i = 0; i < n; i++) b[p++] = t[i];
	b[p] = 0;
	return p;
}

/* Left margin (cols) so the fixed-width bordered UI is centred on the screen
 * instead of sitting in the top-left corner. Set per render from screen_cols(). */
static int g_mx = 0;
static void lead(void) { for (int i = 0; i < g_mx; i++) uart_putc(CONSOLE, ' '); }

/* Emit one bordered row. buf holds UI_W visible chars. If inv_len>0, the span
 * [inv_start, inv_start+inv_len) is drawn inverse ("pressed" button). */
static void emit(const char *buf, int inv_start, int inv_len)
{
	lead();
	uart_printf(CONSOLE, UI_BG UI_FG BX_V);
	if (inv_len <= 0) {
		uart_printf(CONSOLE, "%s", buf);
	} else {
		char tmp[UI_W + 1];
		int i, j;
		for (i = 0; i < inv_start; i++) tmp[i] = buf[i];
		tmp[inv_start] = 0;
		uart_printf(CONSOLE, "%s", tmp);
		for (i = 0; i < inv_len; i++) tmp[i] = buf[inv_start + i];
		tmp[inv_len] = 0;
		uart_printf(CONSOLE, UI_INV_ON "%s" UI_INV_OFF, tmp);
		j = 0;
		for (i = inv_start + inv_len; i < UI_W; i++) tmp[j++] = buf[i];
		tmp[j] = 0;
		uart_printf(CONSOLE, "%s", tmp);
	}
	uart_printf(CONSOLE, BX_V UI_RESET "\r\n");
}

static void rule(const char *ch)   /* horizontal divider of `ch` across UI_W */
{
	lead();
	uart_printf(CONSOLE, UI_BG UI_FG BX_V);
	for (int i = 0; i < UI_W; i++) uart_printf(CONSOLE, "%s", ch);
	uart_printf(CONSOLE, BX_V UI_RESET "\r\n");
}

static void border(const char *l, const char *r)
{
	lead();
	uart_printf(CONSOLE, UI_BG UI_FG "%s", l);
	for (int i = 0; i < UI_W; i++) uart_printf(CONSOLE, BX_H);
	uart_printf(CONSOLE, "%s" UI_RESET "\r\n", r);
}

static void line(const char *content)   /* plain padded content row */
{
	char b[UI_W + 1];
	pclear(b, UI_W);
	pput(b, 0, content, UI_W);
	emit(b, 0, 0);
}

/* "P1 (Tn): [ MOVE ]" with the move shown as a pressed button (inverse), or the
 * bracketed radio selectors when the player has not moved yet. */
static void move_row(const char *who, int tid, int move)
{
	char b[UI_W + 1];
	char num[16];
	int c = 0;
	pclear(b, UI_W);
	pput(b, c, who, UI_W);     c += 2;       /* "P1" / "P2" */
	pput(b, c, " (T", UI_W);   c += 3;
	c += pput_num(b, c, (unsigned)tid, num);
	pput(b, c, "): ", UI_W);   c += 3;

	if (move < 0) {
		/* awaiting: bracketed radios, Layer 4 UI style */
		pput(b, c, "( ) Rock ( ) Paper ( ) Sci", UI_W);
		emit(b, 0, 0);
	} else {
		/* pressed button: [ MOVE ] inverse */
		char btn[16];
		btn[0] = '['; btn[1] = ' ';
		const char *mn = rps_move_name(move);
		int k = 2; for (int i = 0; mn[i]; i++) btn[k++] = mn[i];
		btn[k++] = ' '; btn[k++] = ']'; btn[k] = 0;
		pput(b, c, btn, UI_W);
		emit(b, c, k);
	}
}

void ui_begin(void)
{
	ui_app_enter();
}

void ui_end(void)
{
	ui_app_leave();
}

void ui_render(unsigned uptime)
{
	char b[UI_W + 1];
	char num[16];
	int n;

	uart_printf(CONSOLE, UI_HOME);
	g_mx = (screen_cols() - (UI_W + 2)) / 2; if (g_mx < 0) g_mx = 0;

	border(BX_TL, BX_TR);

	/* ---- inverse title bar: app name left, clock ticks right ---- */
	pclear(b, UI_W);
	pput(b, 1, "RPS Controller OS", UI_W);
	n = putoa(uptime, num);
	pput(b, UI_W - n - 2, num, UI_W);
	pput(b, UI_W - 1, "t", UI_W);
	emit(b, 0, UI_W);   /* whole bar inverse */

	rule(BX_H);

	/* ---- system stats ---- */
	line(" [SYSTEM STATS]");
	pclear(b, UI_W);
	pput(b, 1, "Uptime:", UI_W);
	n = putoa(uptime, num);
	pput(b, 9, num, UI_W);
	pput(b, 9 + n, " t", UI_W);
	pput(b, 22, "Idle Load: lo", UI_W);
	emit(b, 0, 0);

	pclear(b, UI_W);
	pput(b, 1, "Games:", UI_W);
	n = putoa((unsigned)g_rps.games_played, num);
	pput(b, 8, num, UI_W);
	pput(b, 14, "Active Matches:", UI_W);
	putoa((unsigned)g_rps.active_matches, num);
	pput(b, 30, num, UI_W);
	emit(b, 0, 0);

	line("");
	rule(BX_TH);     /* thick Layer 4 UI separating rule */
	line("");

	/* ---- headline match zone ---- */
	if (g_rps.m_active) {
		pclear(b, UI_W);
		pput(b, 1, "[MATCH #1 - T", UI_W);
		n = putoa((unsigned)g_rps.m_p1_tid, num);
		int c = 14; pput(b, c, num, UI_W); c += n;
		pput(b, c, " vs T", UI_W); c += 5;
		n = putoa((unsigned)g_rps.m_p2_tid, num);
		pput(b, c, num, UI_W); c += n;
		pput(b, c, "]", UI_W);
		emit(b, 0, 0);

		pclear(b, UI_W);
		if (g_rps.m_p1_move >= 0 && g_rps.m_p2_move >= 0)
			pput(b, 1, "Status: Resolving round...", UI_W);
		else if (g_rps.m_p1_move >= 0 || g_rps.m_p2_move >= 0)
			pput(b, 1, "Status: Awaiting a move...", UI_W);
		else
			pput(b, 1, "Status: Round start", UI_W);
		emit(b, 0, 0);

		move_row("P1", g_rps.m_p1_tid, g_rps.m_p1_move);
		move_row("P2", g_rps.m_p2_tid, g_rps.m_p2_move);

		pclear(b, UI_W);
		pput(b, 1, "Score P1:", UI_W);
		n = putoa((unsigned)g_rps.p1_wins, num); pput(b, 11, num, UI_W);
		pput(b, 11 + n, " P2:", UI_W);
		n = putoa((unsigned)g_rps.p2_wins, num); pput(b, 20, num, UI_W);
		pput(b, 20 + n, " Tie:", UI_W);
		putoa((unsigned)g_rps.ties, num); pput(b, 29, num, UI_W);
		emit(b, 0, 0);
	} else {
		line(" [no active match]");
		line("");
		line("");
		line("");
	}

	line("");
	rule(BX_TH);
	line(" [ abc ]  GRAFFITI PAD");
	line(" Cmd> rps  (auto-playing demo)");

	border(BX_BL, BX_BR);
}

/* ----------------------------------------------- Layer 4 UI launcher (home) --- */

/* two app cells on one row: "(n) Name        (n) Name" */
static void app_row(int n1, const char *a1, int n2, const char *a2)
{
	char b[UI_W + 1];
	char num[16];
	pclear(b, UI_W);
	pput(b, 2, "(", UI_W); pput_num(b, 3, (unsigned)n1, num); pput(b, 4, ") ", UI_W);
	pput(b, 6, a1, UI_W);
	if (a2) {
		pput(b, 22, "(", UI_W); pput_num(b, 23, (unsigned)n2, num); pput(b, 24, ") ", UI_W);
		pput(b, 26, a2, UI_W);
	}
	emit(b, 0, 0);
}

void ui_home_render(unsigned uptime)
{
	char b[UI_W + 1];
	char num[16];
	int n;

	uart_printf(CONSOLE, UI_HOME);
	g_mx = (screen_cols() - (UI_W + 2)) / 2; if (g_mx < 0) g_mx = 0;
	border(BX_TL, BX_TR);

	/* inverse title bar: "Layer 4 UI"  ...  category "All"  ... clock ticks */
	pclear(b, UI_W);
	pput(b, 1, "Layer 4 UI", UI_W);
	pput(b, 22, "All", UI_W);
	n = putoa(uptime, num);
	pput(b, UI_W - n - 2, num, UI_W);
	pput(b, UI_W - 1, "t", UI_W);
	emit(b, 0, UI_W);

	rule(BX_H);
	line(" Applications");
	app_row(1, "Terminal", 2, "RPS");
	app_row(3, "Snake",    4, "Tetris");
	app_row(5, "Pong",     6, "About");
	app_row(7, "Sys Clock", 0, (const char *)0);
	line("");
	rule(BX_TH);
	line(" [ abc ]  Graffiti");
	line(" Tap an app number (1-7):");
	border(BX_BL, BX_BR);
}

/* ------------------------------------------ interactive RPS (You vs CPU) --- */

static int btn_into(char *b, int pos, int move)   /* "[ Move ]"; return width */
{
	const char *mn = rps_move_name(move);
	int k = pos;
	b[k++] = '['; b[k++] = ' ';
	for (int i = 0; mn[i]; i++) b[k++] = mn[i];
	b[k++] = ' '; b[k++] = ']';
	b[k] = 0;
	return k - pos;
}

static void rps_side_line(char *buf, int cap, const char *who, int move)
{
	int p = 0;
	for (const char *s = who; *s && p < cap - 1; s++) buf[p++] = *s;
	if (p < cap - 1) buf[p++] = ':';
	if (p < cap - 1) buf[p++] = ' ';
	if (move >= 0) {
		p += btn_into(buf, p, move);
	} else {
		for (const char *s = "( . )"; *s && p < cap - 1; s++) buf[p++] = *s;
	}
	buf[p < cap ? p : cap - 1] = 0;
}

static int g_rps_render_first = 1;

void ui_rps_human_reset(void)
{
	g_rps_render_first = 1;
}

void ui_rps_human_render(unsigned uptime, int you, int cpu, int tie,
			   int your_move, int cpu_move, int last_result)
{
	char score[80], round_hdr[80], you_line[80], cpu_line[80], result[80];
	char help1[80], help2[80], status[96], num[16];
	UiElem sep1 = { .type = UI_SEP };
	UiElem sep2 = { .type = UI_SEP };
	UiElem sep3 = { .type = UI_SEP };
	UiElem content[10];
	int rows, cols;
	UiAppShell sh;

	int p = 0;
	for (const char *s = "You:"; *s && p < (int)sizeof(score) - 1; s++) score[p++] = *s;
	if (p < (int)sizeof(score) - 1) score[p++] = ' ';
	p = h_apu(score, p, (unsigned)you);
	if (p < (int)sizeof(score) - 1) score[p++] = ' ';
	for (const char *s = "CPU:"; *s && p < (int)sizeof(score) - 1; s++) score[p++] = *s;
	if (p < (int)sizeof(score) - 1) score[p++] = ' ';
	p = h_apu(score, p, (unsigned)cpu);
	if (p < (int)sizeof(score) - 1) score[p++] = ' ';
	for (const char *s = "Tie:"; *s && p < (int)sizeof(score) - 1; s++) score[p++] = *s;
	if (p < (int)sizeof(score) - 1) score[p++] = ' ';
	p = h_apu(score, p, (unsigned)tie);
	score[p < (int)sizeof(score) ? p : (int)sizeof(score) - 1] = 0;

	p = h_app(round_hdr, 0, " Last round:");
	round_hdr[p] = 0;
	rps_side_line(you_line, (int)sizeof(you_line), "You", your_move);
	rps_side_line(cpu_line, (int)sizeof(cpu_line), "CPU", cpu_move);

	p = h_app(result, 0, "Result: ");
	if (last_result == RPS_R_WIN)       p = h_app(result, p, "YOU WIN!");
	else if (last_result == RPS_R_LOSE) p = h_app(result, p, "CPU wins.");
	else if (last_result == RPS_R_TIE)  p = h_app(result, p, "Tie.");
	else                                p = h_app(result, p, "-");
	result[p] = 0;

	p = h_app(help1, 0, " [ abc ]  Your move:");
	help1[p] = 0;
	p = h_app(help2, 0, " (r)ock  (p)aper  (s)cissors  (q)uit");
	help2[p] = 0;

	p = 0;
	p = h_apu(status, p, uptime);
	if (p < (int)sizeof(status) - 1) status[p++] = 't';
	if (p < (int)sizeof(status) - 1) status[p++] = ' ';
	for (const char *s = "(r)ock (p)aper (s)cissors (q)uit"; *s && p < (int)sizeof(status) - 1; s++)
		status[p++] = *s;
	status[p] = 0;
	(void)num;

	content[0] = sep1;
	content[1] = (UiElem){ .type = UI_TEXT, .text = score };
	content[2] = sep2;
	content[3] = (UiElem){ .type = UI_TEXT, .text = round_hdr };
	content[4] = (UiElem){ .type = UI_TEXT, .text = you_line,
	                       .style = { .flags = your_move >= 0 ? UI_BOLD : 0 } };
	content[5] = (UiElem){ .type = UI_TEXT, .text = cpu_line };
	content[6] = (UiElem){ .type = UI_TEXT, .text = result };
	content[7] = sep3;
	content[8] = (UiElem){ .type = UI_TEXT, .text = help1 };
	content[9] = (UiElem){ .type = UI_TEXT, .text = help2 };

	display_get_size(&rows, &cols);
	ui_app_shell_init(&sh, "RPS - You vs CPU", status, rows, content, 10);
	ui_app_shell_render(&sh, g_rps_render_first);
	g_rps_render_first = 0;
}

/* ----------------------------------------------------------- key input ---- */

unsigned char ui_getch(void)
{
	int clock = ClockServerTid();
	while (!uart_rxc(CONSOLE)) {
		if (clock >= 0) Delay(clock, 1);   /* sleep between polls; CPU idles */
		else Yield();
	}
	return uart_getc(CONSOLE);
}

/* Pending bytes when an escape sequence is incomplete or has trailing data. */
static unsigned char ui_pending[8];
static int ui_pending_n;

static void ui_pending_push(unsigned char c)
{
	if (ui_pending_n < (int)sizeof(ui_pending))
		ui_pending[ui_pending_n++] = c;
}

static int ui_pending_pop(void)
{
	if (ui_pending_n <= 0)
		return -1;
	unsigned char c = ui_pending[0];
	for (int i = 1; i < ui_pending_n; i++)
		ui_pending[i - 1] = ui_pending[i];
	ui_pending_n--;
	return (int)c;
}

static int ui_arrow_from_csi(int c3)
{
	if (c3 == 'A') return UI_KEY_UP;
	if (c3 == 'B') return UI_KEY_DOWN;
	if (c3 == 'C') return UI_KEY_RIGHT;
	if (c3 == 'D') return UI_KEY_LEFT;
	return -1;
}

/* ESC sequence assembly — bytes over the WebSocket bridge can arrive one tick apart. */
static int ui_esc_active;
static unsigned char ui_esc_lead;
static int ui_esc_have_lead;
static int ui_esc_ticks;

#define UI_ESC_WAIT_LEAD  20   /* 200ms for '[' or 'O' after ESC */
#define UI_ESC_WAIT_TAIL  10   /* 100ms for A/B/C/D after lead */

static int ui_read_key(void)
{
	if (ui_pending_n > 0)
		return ui_pending_pop();

	if (ui_esc_active) {
		if (uart_rxc(CONSOLE)) {
			unsigned char b = (unsigned char)uart_getc(CONSOLE);
			ui_esc_ticks = 0;
			if (!ui_esc_have_lead) {
				ui_esc_lead = b;
				ui_esc_have_lead = 1;
				if (b != '[' && b != 'O') {
					ui_esc_active = 0;
					ui_esc_have_lead = 0;
					ui_pending_push(b);
					return 0x1b;
				}
				return -1;
			}
			ui_esc_active = 0;
			ui_esc_have_lead = 0;
			if (ui_esc_lead == '[' || ui_esc_lead == 'O') {
				int arrow = ui_arrow_from_csi((int)b);
				if (arrow >= 0)
					return arrow;
			}
			ui_pending_push(ui_esc_lead);
			ui_pending_push(b);
			return 0x1b;
		}
		ui_esc_ticks++;
		{
			int limit = ui_esc_have_lead ? UI_ESC_WAIT_TAIL : UI_ESC_WAIT_LEAD;
			if (ui_esc_ticks >= limit) {
				ui_esc_active = 0;
				if (!ui_esc_have_lead)
					return 0x1b;
				ui_pending_push(ui_esc_lead);
				ui_esc_have_lead = 0;
				return 0x1b;
			}
		}
		return -1;
	}

	{
		int console_tid = WhoIs(UART1_CONSOLE_SERVER);
		int c;
		if (console_tid > 0) {
			c = ConsolePoll(console_tid);
			if (c < 0)
				return -1;
		} else {
			if (!uart_rxc(CONSOLE))
				return -1;
			c = (unsigned char)uart_getc(CONSOLE);
		}
		if (c != 0x1b)
			return c;
		ui_esc_active = 1;
		ui_esc_have_lead = 0;
		ui_esc_ticks = 0;
		return -1;
	}
}

int ui_trygetch(void)
{
	return ui_read_key();
}

/* Blocking key read that decodes arrow keys (UI_KEY_*). Clock-paced so the CPU
 * idles between polls instead of spinning. */
int ui_getkey(void)
{
	int clock = ClockServerTid();
	for (;;) {
		int k = ui_read_key();
		if (k >= 0)
			return k;
		if (clock >= 0)
			Delay(clock, 1);
		else
			Yield();
	}
}

/* Game-over choice: draw a centred "[R]eplay   [Q]uit" prompt at `row` within the
 * column span [col, col+width), then block for a key. Returns 'r' or 'q'. */
int ui_replay_prompt(int row, int col, int width)
{
	const char *p = "[Enter/R] Replay   [Q] Quit";
	int len = 0; while (p[len]) len++;
	int x = col + (width - len) / 2; if (x < col) x = col;
	display_put_str(row, x, p, "\033[1;37m");
	display_flush();
	for (;;) {
		unsigned char k = ui_getch();
		if (k == 'r' || k == 'R' || k == '\r' || k == '\n') return 'r';
		if (k == 'q' || k == 'Q') return 'q';
	}
}

void ui_getline(char *buf, int max)
{
	int clock = ClockServerTid();
	int len = 0;
	for (;;) {
		while (!uart_rxc(CONSOLE)) {
			if (clock >= 0) Delay(clock, 1); else Yield();
		}
		unsigned char c = uart_getc(CONSOLE);
		if (c == '\r' || c == '\n') { uart_printf(CONSOLE, "\r\n"); break; }
		if (c == 0x7f || c == 0x08) {
			if (len > 0) { len--; uart_printf(CONSOLE, "\b \b"); }
			continue;
		}
		if (c < 0x20) continue;
		if (len < max - 1) { buf[len++] = (char)c; uart_putc(CONSOLE, c); }   /* echo */
	}
	buf[len] = '\0';
}

static unsigned rng_state = 0x2545F491u;
unsigned ui_rng(void)
{
	rng_state = rng_state * 1103515245u + 12345u;
	rng_state ^= (unsigned)Time(ClockServerTid()) << 3;
	return rng_state >> 8;
}

/* ------------------------------------------------- per-app game canvas ---- */

/* ----------------------------------------------- universal screen size ---- */
/* The display server owns g_display_rows/cols. These wrappers cache the last
 * reply so hot paths (layout, rendering) avoid an IPC round trip per read. */
static int g_scr_rows = 0, g_scr_cols = 0;
static int g_scr_manual = 0;

static void screen_cache_update(int rows, int cols, int manual)
{
	if (rows > 0)
		g_scr_rows = rows;
	if (cols > 0)
		g_scr_cols = cols;
	g_scr_manual = manual ? 1 : 0;
}

static void screen_cache_from_reply(int rows, int cols, int manual)
{
	screen_cache_update(rows, cols, manual);
}

void screen_set(int cols, int rows)
{
	display_set_size(cols, rows);
	display_get_size(&g_scr_rows, &g_scr_cols);
	g_scr_manual = display_screen_is_manual();
}

void screen_auto(void)
{
	display_screen_auto();
	display_get_size(&g_scr_rows, &g_scr_cols);
	g_scr_manual = display_screen_is_manual();
}

int screen_is_manual(void)
{
	if (DisplayServerTid() >= 0)
		return display_screen_is_manual();
	return g_scr_manual;
}

void screen_query(void)
{
	int rows = 0, cols = 0;
	display_get_size(&rows, &cols);
	screen_cache_from_reply(rows, cols, display_screen_is_manual());
}

void screen_query_terminal(void)
{
	int rows = 0, cols = 0;
	display_query_terminal(&rows, &cols);
	screen_cache_from_reply(rows, cols, display_screen_is_manual());
}

int screen_rows(void) { if (g_scr_rows <= 0) screen_query(); return g_scr_rows; }
int screen_cols(void) { if (g_scr_cols <= 0) screen_query(); return g_scr_cols; }

int ui_term_cols(void) { screen_query(); return g_scr_cols; }

/* ------------------------------------------------- per-app game canvas ---- */

void ui_canvas_clear(UiCanvas *cv);

void ui_canvas_init(UiCanvas *cv)
{
	cv->w = 56;                   /* defaults; ui_canvas_autofit() sizes both */
	cv->h = 18;
	ui_canvas_clear(cv);
}

int ui_canvas_w(const UiCanvas *cv) { return cv->w; }
int ui_canvas_h(const UiCanvas *cv) { return cv->h; }

void ui_canvas_resize(UiCanvas *cv, int delta)
{
	cv->w += delta;
	if (cv->w < 20) cv->w = 20;
	if (cv->w > UI_CANVAS_WMAX) cv->w = UI_CANVAS_WMAX;
}

/* size the canvas to the whole terminal: width = cols - 2 borders, height =
 * rows - the frame chrome (title bar, rules, status bar). */
void ui_canvas_autofit(UiCanvas *cv)
{
	screen_query();
	int w = g_scr_cols - 2;
	int h = g_scr_rows - UI_CANVAS_CHROME - UI_GAME_BARS;
	if (w < 20) w = 20;
	if (w > UI_CANVAS_WMAX) w = UI_CANVAS_WMAX;
	if (h < UI_CANVAS_HMIN) h = UI_CANVAS_HMIN;
	if (h > UI_CANVAS_HMAX) h = UI_CANVAS_HMAX;
	cv->w = w;
	cv->h = h;
}

void ui_canvas_clear(UiCanvas *cv)
{
	for (int r = 0; r < cv->h; r++)
		for (int c = 0; c < cv->w; c++) {
			cv->cells[r][c] = ' ';
			cv->fg[r][c] = 0;
			cv->g3[r][c][0] = 0;
			cv->g3[r][c][1] = 0;
			cv->g3[r][c][2] = 0;
		}
}

void ui_canvas_put_cell(UiCanvas *cv, int row, int col, char ch)
{
	if (row < 0 || row >= cv->h || col < 0 || col >= cv->w) return;
	cv->cells[row][col] = ch;
	cv->fg[row][col] = 0;
	cv->g3[row][col][0] = 0;
}

void ui_canvas_put_glyph(UiCanvas *cv, int row, int col,
                         unsigned char b0, unsigned char b1, unsigned char b2,
                         unsigned char fg)
{
	if (row < 0 || row >= cv->h || col < 0 || col >= cv->w) return;
	cv->g3[row][col][0] = b0;
	cv->g3[row][col][1] = b1;
	cv->g3[row][col][2] = b2;
	cv->cells[row][col] = ' ';
	cv->fg[row][col] = fg;
}

void ui_canvas_emit_cell(const UiCanvas *cv, int row, int col, char ch)
{
	if (row < 0 || row >= cv->h || col < 0 || col >= cv->w) return;
	uart_printf(CONSOLE, "\033[%d;%dH%c", CANVAS_BODY_ROW + row, CANVAS_BODY_COL + col, ch);
}

/* width-aware horizontal line: left + w dashes + right. No Palm LCD tint — the
 * game canvas uses the terminal's own background (the Palm theme stays on the
 * RPS/launcher screens, which render via emit/line/border). */
static void cw_hline(int w, const char *l, const char *r)
{
	uart_printf(CONSOLE, "%s", l);
	for (int i = 0; i < w; i++) uart_printf(CONSOLE, BX_H);
	uart_printf(CONSOLE, "%s" UI_RESET "\r\n", r);
}

/* width-aware content row (buf must be exactly w chars, NUL-terminated) */
static void cw_row(const char *buf, int inverse)
{
	if (inverse)
		uart_printf(CONSOLE, BX_V UI_INV_ON "%s" UI_INV_OFF BX_V UI_RESET "\r\n", buf);
	else
		uart_printf(CONSOLE, BX_V "%s" BX_V UI_RESET "\r\n", buf);
}

void ui_canvas_draw(const UiCanvas *cv, const char *title, unsigned uptime, const char *status)
{
	int w = cv->w;
	char b[UI_CANVAS_WMAX + 1];
	char num[16];
	int n, i;

	uart_printf(CONSOLE, UI_SYNC_BEGIN "\033[%d;1H", UI_GAME_BARS + 1);
	cw_hline(w, BX_TL, BX_TR);

	/* inverse title bar: title left, uptime right */
	for (i = 0; i < w; i++) b[i] = ' ';
	b[w] = 0;
	for (i = 0; title[i] && 1 + i < w; i++) b[1 + i] = title[i];
	n = putoa(uptime, num);
	for (i = 0; i < n && (w - n - 2 + i) >= 0; i++) b[w - n - 2 + i] = num[i];
	if (w >= 1) b[w - 1] = 't';
	cw_row(b, 1);

	cw_hline(w, BX_V, BX_V);
	for (int row = 0; row < cv->h; row++) {
		for (i = 0; i < w; i++) b[i] = cv->cells[row][i];
		b[w] = 0;
		cw_row(b, 0);
	}
	cw_hline(w, BX_V, BX_V);

	for (i = 0; i < w; i++) b[i] = ' ';
	b[w] = 0;
	for (i = 0; status[i] && i < w; i++) b[i] = status[i];
	cw_row(b, 0);

	cw_hline(w, BX_BL, BX_BR);
	uart_printf(CONSOLE, UI_SYNC_END);
}

void ui_canvas_draw_status(const UiCanvas *cv, const char *status)
{
	int w = cv->w;
	char b[UI_CANVAS_WMAX + 1];
	int i;

	for (i = 0; i < w; i++) b[i] = ' ';
	for (i = 0; status[i] && i < w; i++) b[i] = status[i];
	b[w] = 0;
	uart_printf(CONSOLE, "\033[%d;1H", CANVAS_BODY_ROW + cv->h + 1);
	cw_row(b, 0);
}

void ui_canvas_draw_uptime(const UiCanvas *cv, unsigned uptime)
{
	char num[16];
	int n = putoa(uptime, num);
	int col = CANVAS_BODY_COL + cv->w - n - 2;
	if (col < CANVAS_BODY_COL + 1) col = CANVAS_BODY_COL + 1;
	uart_printf(CONSOLE, "\033[%d;%dH\033[7m", CANVAS_TOP_ROW + 1, col);
	for (int i = 0; i < n; i++) uart_printf(CONSOLE, "%c", num[i]);
	uart_printf(CONSOLE, "t\033[0m");
}

void ui_draw_stats_line(int row, int col)
{
	char st[32];
	build_stats(st);
	uart_printf(CONSOLE, "\033[%d;%dH\033[90m%s\033[0m", row, col, st);
}

/* Legacy UART bar path — UI_SCREEN auto-prepends UI_METERS in the display server. */
void ui_draw_game_bars(void) { (void)0; }
void ui_draw_game_bars_invalidate(void) { (void)0; }

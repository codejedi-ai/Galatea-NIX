/*
 * DarcyOS APU terminal shell — raw UART console, no display/UI layer.
 */
#include "rpi.h"
#include "syscall.h"
#include "string.h"
#include "pmm.h"
#include "project.h"
#include "config.h"
#include "clockserver.h"
#include "clock_client.h"
#include "nameserver.h"
#include "../layer3-services/uart/io_api/io_api.h"
#include "../layer3-services/uart/UART1_CONSOLE_server/UART1_CONSOLE_server.h"
#include "../layer3-services/apu_client.h"
#include "../layer3-services/apu_core_server.h"
#include "accel.h"
#if ENABLE_MARKLIN == 1
#include "marklin.h"
#include "tc1.h"
#endif

#define LINE_MAX 128

static int streq(const char *a, const char *b)
{
	while (*a && *b) {
		if (*a != *b) return 0;
		a++; b++;
	}
	return *a == *b;
}

static char *split_args(char *line)
{
	char *p = line;
	while (*p && *p != ' ') p++;
	if (*p == '\0') return p;
	*p = '\0';
	return p + 1;
}

static void term_puts(const char *s)
{
	uart_puts(CONSOLE, (char *)s);
}

static unsigned char term_getc(void)
{
	int clock_tid = ClockServerTid();
	int console_tid = WhoIs(UART1_CONSOLE_SERVER);
	if (console_tid > 0)
		return (unsigned char)ConsoleGetc(console_tid);
	while (!uart_rxc(CONSOLE)) {
		if (clock_tid >= 0)
			Delay(clock_tid, 1);
		else
			Yield();
	}
	return uart_getc(CONSOLE);
}

static void term_readline(char *buf, int max)
{
	int len = 0;
	buf[0] = '\0';
	for (;;) {
		unsigned char c = term_getc();
		if (c == '\r' || c == '\n') {
			term_puts("\r\n");
			break;
		}
		if (c == 0x7f || c == 0x08) {
			if (len > 0) {
				len--;
				buf[len] = '\0';
				term_puts("\b \b");
			}
			continue;
		}
		if (c < 0x20) continue;
		if (len == 0 && (c == ' ' || c == '\t')) continue;
		if (len < max - 1) {
			buf[len++] = (char)c;
			uart_putc(CONSOLE, c);
		}
	}
	buf[len] = '\0';
}

static void cmd_help(void)
{
	uart_printf(CONSOLE, "DarcyOS APU terminal commands:\r\n");
	uart_printf(CONSOLE, "  help            show this help\r\n");
	uart_printf(CONSOLE, "  echo <text>     print text\r\n");
	uart_printf(CONSOLE, "  pages           physical frame pool stats\r\n");
	uart_printf(CONSOLE, "  uptime          kernel runtime ticks\r\n");
	uart_printf(CONSOLE, "  tid             this shell task id\r\n");
	uart_printf(CONSOLE, "  clear           ANSI clear screen\r\n");
	uart_printf(CONSOLE, "  apu             show APU core stats\r\n");
	uart_printf(CONSOLE, "  aputest         run parallel APU smoke test\r\n");
#if ENABLE_MARKLIN == 1
	uart_printf(CONSOLE, "  tc1 start|stop|goto|speed  train control\r\n");
#endif
}

static void cmd_apu(void)
{
	for (int c = 1; c <= 3; c++) {
		uart_printf(CONSOLE, "  APU%d: busy=%d jobs_done=%u tid=%d\r\n",
		            c, accel_is_busy(c), accel_jobs_done(c),
		            APUCoreServerTid(c));
	}
}

static volatile unsigned apu_test_results[3];

static void apu_test_worker(void *arg)
{
	unsigned idx = *(unsigned *)arg;
	apu_test_results[idx] = 0xDAAC0000u + idx;
}

static void cmd_aputest(void)
{
	unsigned indices[3] = { 0, 1, 2 };
	ApuJob jobs[3];

	apu_test_results[0] = apu_test_results[1] = apu_test_results[2] = 0;
	for (int i = 0; i < 3; i++) {
		jobs[i].fn  = apu_test_worker;
		jobs[i].arg = &indices[i];
	}
	int n = APUBatch(jobs, 3);
	uart_printf(CONSOLE, "aputest: batch dispatched %d jobs\r\n", n);
	for (int i = 0; i < 3; i++) {
		unsigned expect = 0xDAAC0000u + (unsigned)i;
		uart_printf(CONSOLE, "  core %d result=0x%x %s\r\n",
		            i + 1, apu_test_results[i],
		            apu_test_results[i] == expect ? "OK" : "FAIL");
	}
}

static void demo_program(void)
{
	uart_printf(CONSOLE, "  [demo] tid=%d\r\n", MyTid());
	uint64_t f = 1;
	for (int i = 1; i <= 10; i++) f *= (uint64_t)i;
	uart_printf(CONSOLE, "  [demo] 10! = %u\r\n", (unsigned)f);
	Exit();
}

static void shell_loop(void)
{
	char line[LINE_MAX];

	for (;;) {
		uart_printf(CONSOLE, "\033[1;32m%s\033[0m@\033[1;36m%s\033[0m$ ",
		            PROJECT_USER, PROJECT_HOSTNAME);
		term_readline(line, LINE_MAX);
		if (line[0] == '\0') continue;

		char *args = split_args(line);

		if (streq(line, "help")) {
			cmd_help();
		} else if (streq(line, "echo")) {
			uart_printf(CONSOLE, "%s\r\n", args);
		} else if (streq(line, "pages")) {
			uint64_t total = pmm_total_pages();
			uint64_t freep = pmm_free_pages_count();
			uart_printf(CONSOLE,
			    "frames: total=%u used=%u free=%u (page=%u B)\r\n",
			    (unsigned)total, (unsigned)(total - freep),
			    (unsigned)freep, (unsigned)PAGE_SIZE);
		} else if (streq(line, "run")) {
			int t = Create(10, demo_program);
			uart_printf(CONSOLE, "created demo task tid=%d\r\n", t);
			for (int i = 0; i < 30; i++) Yield();
		} else if (streq(line, "uptime")) {
			uart_printf(CONSOLE, "uptime: %u ticks\r\n",
			            (unsigned)GetKernelRuntime());
		} else if (streq(line, "tid")) {
			uart_printf(CONSOLE, "tid: %d\r\n", MyTid());
		} else if (streq(line, "clear")) {
			term_puts("\033[2J\033[H");
		} else if (streq(line, "apu")) {
			cmd_apu();
		} else if (streq(line, "aputest")) {
			cmd_aputest();
#if ENABLE_MARKLIN == 1
		} else if (streq(line, "tc1")) {
			char *sub = split_args(args);
			if (streq(args, "start")) {
				if (TC1Tid() >= 0)
					uart_printf(CONSOLE, "tc1: already running (tid %d)\r\n", TC1Tid());
				else {
					int tid = Create(TC1_PRIORITY, tc1_entry);
					uart_printf(CONSOLE, "tc1: started (tid %d)\r\n", tid);
				}
			} else if (streq(args, "goto")) {
				if (!sub || !sub[0])
					uart_printf(CONSOLE, "usage: tc1 goto <sensor>\r\n");
				else if (TC1Tid() < 0)
					uart_printf(CONSOLE, "tc1: not running\r\n");
				else
					TC1Goto(sub);
			} else if (streq(args, "speed")) {
				if (!sub || !sub[0])
					uart_printf(CONSOLE, "usage: tc1 speed <0-14>\r\n");
				else if (TC1Tid() < 0)
					uart_printf(CONSOLE, "tc1: not running\r\n");
				else {
					int sp = 0;
					for (int i = 0; sub[i] >= '0' && sub[i] <= '9'; i++)
						sp = sp * 10 + (sub[i] - '0');
					if (sp > 14) sp = 14;
					TC1Speed(sp);
				}
			} else if (streq(args, "stop")) {
				if (TC1Tid() < 0)
					uart_printf(CONSOLE, "tc1: not running\r\n");
				else
					TC1Speed(0);
			} else {
				uart_printf(CONSOLE,
				    "usage: tc1 start|stop|goto <sensor>|speed <0-14>\r\n");
			}
#endif
		} else {
			uart_printf(CONSOLE, "unknown: '%s' (try 'help')\r\n", line);
		}
	}
}

void terminal_shell_entry(void)
{
	uart_printf(CONSOLE, "\r\n\033[1;36m[ DarcyOS APU ]\033[0m ");
	uart_printf(CONSOLE, "%s — terminal-only (no UI heap)\r\n", PROJECT_DISPLAY_NAME);
	uart_printf(CONSOLE, "APU servers: %s, %s, %s\r\n",
	            APU1_SERVER_NAME, APU2_SERVER_NAME, APU3_SERVER_NAME);
	shell_loop();
}

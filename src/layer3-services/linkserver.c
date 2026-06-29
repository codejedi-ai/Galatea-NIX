#include "linkserver.h"
#include "nameserver.h"
#include "clockserver.h"
#include "clock_client.h"
#include "../layer2-messaging/messaging.h"
#include "../layer1-processes/syscall.h"
#include "../layer1-processes/auxuart.h"

#define LINK_BUF            128   /* RX chunk / command buffer in a message */
#define LINK_TIMEOUT_TICKS  300   /* ~3 s (clock ticks are 10 ms) */

typedef enum {
	LINK_MSG_RX   = 1,   /* notifier -> server: drained UART bytes */
	LINK_MSG_SEND = 2,   /* client   -> server: a command to transmit */
} LinkMsgType;

typedef struct {
	int  type;
	int  len;               /* RX: byte count; SEND: command length */
	char buf[LINK_BUF];     /* RX: raw bytes; SEND: command string  */
} LinkMsg;

static int link_server_tid = -1;
int LinkServerTid(void) { return link_server_tid; }

/* small libc-free helper: copy s into d (cap max), NUL-terminate, return length */
static int l_put(char *d, int max, const char *s)
{
	int n = 0; while (s[n] && n < max - 1) { d[n] = s[n]; n++; } d[n] = 0; return n;
}

/* ----------------------------------------------------------- notifier ----- */

void link_notifier_entry(void)
{
	int server = link_server_tid;
	int clock  = ClockServerTid();
	LinkMsg m;
	int ack;

	for (;;) {
		int n = 0, b;
		while (n < LINK_BUF && (b = auxuart_getc_nb()) >= 0)
			m.buf[n++] = (char)b;
		m.type = LINK_MSG_RX;
		m.len  = n;
		Send(server, (const char *)&m, (int)sizeof(m), (char *)&ack, (int)sizeof(ack));
		if (n == 0)
			Delay(clock, 1);   /* idle: yield ~10 ms; busy: keep draining */
	}
}

/* ------------------------------------------------------------- server ----- */

void link_server_entry(void)
{
	int tid;
	LinkMsg m;
	int ack = 0;
	int clock;

	int  client   = -1;            /* tid awaiting a reply, or -1 */
	int  deadline = 0;             /* clock time at which to give up */
	char line[LINK_REPLY_MAX + 1]; /* the response line being assembled */
	int  llen = 0;

	link_server_tid = MyTid();
	RegisterAs(LINK_SERVER_NAME);
	auxuart_init();
	clock = ClockServerTid();

	Create(LINK_NOTIFIER_PRIORITY, link_notifier_entry);

	for (;;) {
		Receive(&tid, (char *)&m, (int)sizeof(m));

		if (m.type == LINK_MSG_RX) {
			for (int i = 0; i < m.len; i++) {
				char ch = m.buf[i];
				if (ch == '\r')
					continue;
				if (ch == '\n') {
					if (client >= 0) {
						line[llen] = 0;
						Reply(client, line, llen + 1);
						client = -1;
					}
					llen = 0;
				} else if (llen < LINK_REPLY_MAX) {
					line[llen++] = ch;
				} else {
					llen = 0;   /* overlong line: drop and resync */
				}
			}
			if (client >= 0 && Time(clock) >= deadline) {
				char to[16]; int n = l_put(to, sizeof(to), "(no response)");
				Reply(client, to, n + 1);
				client = -1;
				llen = 0;
			}
			Reply(tid, &ack, (int)sizeof(ack));   /* release the notifier */
		} else { /* LINK_MSG_SEND */
			if (client >= 0) {
				char bz[8]; int n = l_put(bz, sizeof(bz), "(busy)");
				Reply(tid, bz, n + 1);
			} else {
				client = tid;
				llen = 0;
				for (int i = 0; m.buf[i] && i < LINK_CMD_MAX; i++)
					auxuart_putc((unsigned char)m.buf[i]);
				auxuart_putc('\n');
				deadline = Time(clock) + LINK_TIMEOUT_TICKS;
				/* reply deferred until the response line arrives */
			}
		}
	}
}

/* -------------------------------------------------------- client stub ----- */

int LinkSend(const char *cmd, char *out, int outmax)
{
	LinkMsg req;
	int n;

	if (link_server_tid < 0) {
		n = l_put(out, outmax, "(no link)");
		return n;
	}
	req.type = LINK_MSG_SEND;
	req.len  = l_put(req.buf, (int)sizeof(req.buf), cmd);
	n = Send(link_server_tid, (const char *)&req, (int)sizeof(req), out, outmax);
	if (n < 0) { out[0] = 0; return 0; }
	if (n > 0 && n <= outmax) out[n - 1] = 0;   /* ensure NUL-terminated */
	else if (outmax > 0)      out[outmax - 1] = 0;
	return n > 0 ? n - 1 : 0;
}

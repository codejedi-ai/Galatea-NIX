#include "clockserver.h"
#include "clock_messages.h"
#include "nameserver.h"
#include "../layer2-messaging/messaging.h"
#include "../layer1-processes/syscall.h"
#include "../layer1-processes/config.h"
#include "../layer1-processes/rpi.h"
#include "../library/string.h"

typedef struct {
	int client_tid;
	int delta;
} ClockDelayNode;

static ClockDelayNode delay_q[CLOCK_MAX_DELAY];
static int delay_n;
static uint32_t clock_ticks;
static int notifier_tid = -1;
/* Static handle so clients/notifier reach the clock server WITHOUT a name
 * server (the name server is shelved). Set by clock_server_entry before it
 * spawns the notifier, so it is always valid by the time anyone reads it. */
static int clock_server_tid = -1;

uint32_t ClockServerTicks(void)
{
	return clock_ticks;
}

int ClockServerTid(void)
{
	return clock_server_tid;
}

static void delay_from_abs(uint32_t now, const uint32_t *abs, const int *tids, int n)
{
	uint32_t prev = now;

	delay_n = n;
	for (int i = 0; i < n; i++) {
		delay_q[i].client_tid = tids[i];
		delay_q[i].delta = (int)(abs[i] - prev);
		prev = abs[i];
	}
}

static void delay_build_abs(uint32_t now, uint32_t *abs, int *tids, int *n)
{
	uint32_t t = now;

	*n = delay_n;
	for (int i = 0; i < delay_n; i++) {
		t += (uint32_t)delay_q[i].delta;
		abs[i] = t;
		tids[i] = delay_q[i].client_tid;
	}
}

static int delay_insert(int client_tid, uint32_t now, uint32_t abs_wake)
{
	uint32_t abs[CLOCK_MAX_DELAY];
	int tids[CLOCK_MAX_DELAY];
	int n;
	int i;

	if (delay_n >= CLOCK_MAX_DELAY)
		return -1;
	if (abs_wake < now)
		abs_wake = now;

	delay_build_abs(now, abs, tids, &n);
	for (i = 0; i < n; i++) {
		if (abs_wake <= abs[i])
			break;
	}
	if (i < n) {
		for (int j = n; j > i; j--) {
			abs[j] = abs[j - 1];
			tids[j] = tids[j - 1];
		}
	}
	abs[i] = abs_wake;
	tids[i] = client_tid;
	n++;
	delay_from_abs(now, abs, tids, n);
	return 0;
}

static void delay_on_tick(void)
{
	int ack = 0;

	if (delay_n == 0)
		return;
	delay_q[0].delta--;
	while (delay_n > 0 && delay_q[0].delta <= 0) {
		Reply(delay_q[0].client_tid, &ack, (int)sizeof(ack));
		delay_n--;
		if (delay_n > 0)
			memmove(&delay_q[0], &delay_q[1],
				(size_t)delay_n * sizeof(ClockDelayNode));
	}
}

static void handle_client(int client_tid, const ClockMsg *msg)
{
	int reply_ticks;

	switch (msg->type) {
	case CLOCK_MSG_TIME:
		reply_ticks = (int)clock_ticks;
		Reply(client_tid, &reply_ticks, (int)sizeof(reply_ticks));
		break;
	case CLOCK_MSG_DELAY:
		if (delay_insert(client_tid, clock_ticks, clock_ticks + (uint32_t)msg->ticks) < 0) {
			reply_ticks = -1;
			Reply(client_tid, &reply_ticks, (int)sizeof(reply_ticks));
		}
		break;
	case CLOCK_MSG_DELAY_UNTIL:
		if (delay_insert(client_tid, clock_ticks, (uint32_t)msg->ticks) < 0) {
			reply_ticks = -1;
			Reply(client_tid, &reply_ticks, (int)sizeof(reply_ticks));
		}
		break;
	default:
		reply_ticks = -1;
		Reply(client_tid, &reply_ticks, (int)sizeof(reply_ticks));
		break;
	}
}

/* Notifier: sleeps on hardware via AwaitEvent, signals server via Send. */
void clock_notifier_entry(void)
{
	int server_tid;
	ClockMsg tick = { CLOCK_MSG_TICK, 0 };
	int ack;

	server_tid = clock_server_tid;   /* set by the server before we were created */
	if (server_tid < 0) {
		uart_printf(CONSOLE, "\033[1;31m[ FAIL ]\033[0m Clock Notifier: no server TID\r\n");
		Exit();
	}

	for (;;) {
		(void)AwaitEvent(CLOCKINTID);
		Send(server_tid, (const char *)&tick, (int)sizeof(tick),
		     (char *)&ack, (int)sizeof(ack));
	}
}

/* Server: never touches hardware; only Receive / Reply. */
void clock_server_entry(void)
{
	int tid;
	char msgbuf[sizeof(ClockMsg)];
	ClockMsg *msg = (ClockMsg *)msgbuf;
	int ack = 0;

	/* Reset static state: BSS is not re-zeroed on warm reboot. */
	delay_n = 0;
	clock_ticks = 0;
	notifier_tid = -1;

	clock_server_tid = MyTid();      /* static handle for the notifier */
	RegisterAs(CLOCK_SERVER_NAME);   /* discoverable via WhoIs (CS452 name server) */

	notifier_tid = Create(CLOCK_NOTIFIER_PRIORITY, clock_notifier_entry);
	uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Clock Server (TID=%d, notifier=%d)\r\n",
		    MyTid(), notifier_tid);

	for (;;) {
		Receive(&tid, msgbuf, (int)sizeof(ClockMsg));
		if (tid == notifier_tid && msg->type == CLOCK_MSG_TICK) {
			clock_ticks++;
			delay_on_tick();
			Reply(tid, &ack, (int)sizeof(ack));
		} else {
			handle_client(tid, msg);
		}
	}
}

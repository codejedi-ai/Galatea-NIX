#include "nameserver.h"
#include "clock_messages.h"
#include "../layer2-messaging/messaging.h"
#include "../layer1-processes/syscall.h"
#include "../layer1-processes/config.h"
#include "../layer1-processes/rpi.h"
#include "../library/string.h"
#include "clockserver.h"
#include "idle.h"
#include "../layer1-processes/shell.h"

void master_test_runner(void);

static struct {
	char name[NS_NAME_MAX];
	int tid;
} ns_table[NS_MAX_NAMES];

static int ns_count;
static int ns_tid = -1;

int NameServerTid(void)
{
	return ns_tid;
}

static int ns_find(const char *name)
{
	for (int i = 0; i < ns_count; i++) {
		if (strncmp(ns_table[i].name, name, NS_NAME_MAX) == 0)
			return ns_table[i].tid;
	}
	return -1;
}

static int ns_add(const char *name, int tid)
{
	if (ns_count >= NS_MAX_NAMES)
		return -1;
	if (ns_find(name) >= 0)
		return -2;
	strncpy(ns_table[ns_count].name, name, NS_NAME_MAX - 1);
	ns_table[ns_count].name[NS_NAME_MAX - 1] = '\0';
	ns_table[ns_count].tid = tid;
	ns_count++;
	return 0;
}

void RegisterAs(const char *name)
{
	NameServerMsg req;
	int reply = -1;

	if (ns_tid < 0)
		return;
	req.type = NS_MSG_REGISTER;
	strncpy(req.name, name, NS_NAME_MAX - 1);
	req.name[NS_NAME_MAX - 1] = '\0';
	Send(ns_tid, (const char *)&req, (int)sizeof(req), (char *)&reply, (int)sizeof(reply));
}

int WhoIs(const char *name)
{
	NameServerMsg req;
	int reply = -1;

	if (ns_tid < 0)
		return -1;
	req.type = NS_MSG_WHOIS;
	strncpy(req.name, name, NS_NAME_MAX - 1);
	req.name[NS_NAME_MAX - 1] = '\0';
	Send(ns_tid, (const char *)&req, (int)sizeof(req), (char *)&reply, (int)sizeof(reply));
	return reply;
}

void name_server_entry(void)
{
	int tid;
	NameServerMsg msg;
	int reply;

	ns_count = 0;
	ns_tid = MyTid();
	ns_add(NAME_SERVER_NAME, ns_tid);

	uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Name Server (TID=%d)\r\n", ns_tid);

	/* Registry only: kmain creates the clock server, idle, and test runner. */
	for (;;) {
		Receive(&tid, (char *)&msg, (int)sizeof(msg));
		reply = -1;
		if (msg.type == NS_MSG_REGISTER) {
			reply = ns_add(msg.name, tid);
		} else if (msg.type == NS_MSG_WHOIS) {
			reply = ns_find(msg.name);
		}
		Reply(tid, &reply, (int)sizeof(reply));
	}
}

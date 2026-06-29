#include "UART1_CONSOLE_server.h"
#include "io_api.h"
#include "../../layer1-processes/processes.h"
#include "../../layer1-processes/rpi.h"
#include "../../layer1-processes/syscall.h"
#include "../../nameserver.h"

#define DISPLAY 1
#define QUEUELENGTH 100
#define CONSOLE_LOG_SIZE 256
#define MAX_CONSOLE_OBSERVERS 8

struct intFun
{
	int tid;
	uint8_t type;
	uint8_t channel;
	uint8_t char_ch;
	uint8_t char_ch2;
};

struct fi_list
{
	struct intFun call[QUEUELENGTH];
	int size;
	uint8_t begin;
	uint8_t end;
};

static struct fi_list console_call_list;
static struct fi_list console_interrupt_list;

struct console_observer
{
	int tid;
	uint8_t mask;
};

static struct console_observer observers[MAX_CONSOLE_OBSERVERS];
static int observer_count;

static char rx_log[CONSOLE_LOG_SIZE];
static char tx_log[CONSOLE_LOG_SIZE];
static uint16_t rx_head;
static uint16_t rx_tail;
static uint16_t rx_count;
static uint16_t tx_head;
static uint16_t tx_tail;
static uint16_t tx_count;

static void rx_log_push(char c)
{
	rx_log[rx_tail] = c;
	rx_tail = (rx_tail + 1) % CONSOLE_LOG_SIZE;
	if (rx_count < CONSOLE_LOG_SIZE)
		rx_count++;
	else
		rx_head = (rx_head + 1) % CONSOLE_LOG_SIZE;
}

static void tx_log_push(char c)
{
	tx_log[tx_tail] = c;
	tx_tail = (tx_tail + 1) % CONSOLE_LOG_SIZE;
	if (tx_count < CONSOLE_LOG_SIZE)
		tx_count++;
	else
		tx_head = (tx_head + 1) % CONSOLE_LOG_SIZE;
}

static void notify_observers(uint8_t event_mask, char ch)
{
	char msg[8];
	char dummy;
	int i;

	msg[0] = IO_CONSOLE_NOTIFY;
	msg[1] = CONSOLE;
	msg[2] = event_mask;
	msg[3] = (uint8_t)ch;
	msg[4] = 0;
	msg[5] = 0;
	msg[6] = 0;
	msg[7] = 0;

	for (i = 0; i < observer_count; i++)
	{
		if (observers[i].mask & event_mask)
			Send(observers[i].tid, msg, 8, &dummy, 0);
	}
}

static int register_observer(int tid, uint8_t mask)
{
	int i;

	for (i = 0; i < observer_count; i++)
	{
		if (observers[i].tid == tid)
		{
			observers[i].mask = mask;
			return 0;
		}
	}
	if (observer_count >= MAX_CONSOLE_OBSERVERS)
		return -1;

	observers[observer_count].tid = tid;
	observers[observer_count].mask = mask;
	observer_count++;
	return 0;
}

static int unregister_observer(int tid)
{
	int i;

	for (i = 0; i < observer_count; i++)
	{
		if (observers[i].tid == tid)
		{
			observers[i] = observers[observer_count - 1];
			observer_count--;
			return 0;
		}
	}
	return -1;
}

static void match_getc(struct fi_list *call_list, struct fi_list *interrupt_list)
{
	char reply[8];
	int ret_pid;

	if (call_list->size <= 0 || interrupt_list->size <= 0)
		return;

	ret_pid = call_list->call[call_list->begin].tid;
	reply[0] = interrupt_list->call[interrupt_list->begin].type;
	reply[1] = interrupt_list->call[interrupt_list->begin].channel;
	reply[2] = interrupt_list->call[interrupt_list->begin].char_ch;
	reply[3] = interrupt_list->call[interrupt_list->begin].char_ch2;
	Reply(ret_pid, reply, 8);

	call_list->call[call_list->begin].tid = 0;
	call_list->begin = (call_list->begin + 1) % QUEUELENGTH;
	call_list->size--;

	interrupt_list->call[interrupt_list->begin].tid = 0;
	interrupt_list->begin = (interrupt_list->begin + 1) % QUEUELENGTH;
	interrupt_list->size--;
}

void UART1_CONSOLE_server(void)
{
	int tid;
	char recieve[8];

	console_call_list.size = 0;
	console_interrupt_list.size = 0;
	observer_count = 0;
	rx_head = rx_tail = rx_count = 0;
	tx_head = tx_tail = tx_count = 0;

	RegisterAs(UART1_CONSOLE_SERVER);

	while (1)
	{
		int io_notifier_tid = WhoIs("io_notifier");
		uint8_t type;
		uint8_t channel;
		uint8_t char_ch;

		Receive(&tid, recieve, 8);
		type = recieve[0];
		channel = recieve[1];
		char_ch = recieve[2];

		if (io_notifier_tid == tid)
			Reply(tid, recieve, 0);

		if (type == RXIC && channel == CONSOLE)
		{
			rx_log_push((char)char_ch);
			notify_observers(CONSOLE_OBSERVE_RX, (char)char_ch);

			console_interrupt_list.call[console_interrupt_list.end].tid = tid;
			console_interrupt_list.call[console_interrupt_list.end].type = type;
			console_interrupt_list.call[console_interrupt_list.end].channel = channel;
			console_interrupt_list.call[console_interrupt_list.end].char_ch = char_ch;
			console_interrupt_list.end = (console_interrupt_list.end + 1) % QUEUELENGTH;
			console_interrupt_list.size++;
			match_getc(&console_call_list, &console_interrupt_list);
		}
		else if (type == TXIC && channel == CONSOLE)
		{
			#if DISPLAY % 3 == 0
			uart_printf(CONSOLE, "	console TXIC\r\n");
			#endif
		}
		else if (type == IO_GETC && channel == CONSOLE)
		{
			console_call_list.call[console_call_list.end].tid = tid;
			console_call_list.call[console_call_list.end].type = type;
			console_call_list.call[console_call_list.end].channel = channel;
			console_call_list.call[console_call_list.end].char_ch = char_ch;
			console_call_list.end = (console_call_list.end + 1) % QUEUELENGTH;
			console_call_list.size++;
			match_getc(&console_call_list, &console_interrupt_list);
		}
		else if (type == IO_PEEK && channel == CONSOLE)
		{
			char reply[8];
			reply[0] = IO_PEEK;
			reply[1] = CONSOLE;
			if (console_interrupt_list.size > 0)
				reply[2] = console_interrupt_list.call[console_interrupt_list.begin].char_ch;
			else
				reply[2] = (char)-1;
			reply[3] = 0;
			Reply(tid, reply, 8);
		}
		else if (type == IO_PUTC && channel == CONSOLE)
		{
			tx_log_push((char)char_ch);
			notify_observers(CONSOLE_OBSERVE_TX, (char)char_ch);
			uart_putc(CONSOLE, char_ch);
			recieve[2] = char_ch;
			Reply(tid, recieve, 8);
		}
		else if (type == IO_CONSOLE_OBSERVE)
		{
			int ret = register_observer(tid, char_ch);
			recieve[2] = (ret == 0) ? 0 : (char)-1;
			Reply(tid, recieve, 8);
		}
		else if (type == IO_CONSOLE_UNOBSERVE)
		{
			int ret = unregister_observer(tid);
			recieve[2] = (ret == 0) ? 0 : (char)-1;
			Reply(tid, recieve, 8);
		}
	}
	Exit();
}

int ConsoleServerTid(void)
{
	return WhoIs(UART1_CONSOLE_SERVER);
}

static int console_send(int server_tid, char *msg)
{
	if (server_tid < 0)
		return -1;
	Send(server_tid, msg, 8, msg, 8);
	return (signed char)msg[2];
}

int ConsoleGetc(int server_tid)
{
	char msg[8];
	msg[0] = IO_GETC;
	msg[1] = CONSOLE;
	msg[2] = 0;
	msg[3] = 0;
	return console_send(server_tid, msg);
}

int ConsolePoll(int server_tid)
{
	char msg[8];
	msg[0] = IO_PEEK;
	msg[1] = CONSOLE;
	msg[2] = 0;
	msg[3] = 0;
	return console_send(server_tid, msg);
}

int ConsolePutc(int server_tid, unsigned char ch)
{
	char msg[8];
	msg[0] = IO_PUTC;
	msg[1] = CONSOLE;
	msg[2] = ch;
	msg[3] = 0;
	return console_send(server_tid, msg);
}

int ConsoleObserve(int server_tid, uint8_t mask)
{
	char msg[8];
	msg[0] = IO_CONSOLE_OBSERVE;
	msg[1] = CONSOLE;
	msg[2] = mask;
	msg[3] = 0;
	Send(server_tid, msg, 8, msg, 8);
	return msg[2];
}

int ConsoleUnobserve(int server_tid)
{
	char msg[8];
	msg[0] = IO_CONSOLE_UNOBSERVE;
	msg[1] = CONSOLE;
	msg[2] = 0;
	msg[3] = 0;
	Send(server_tid, msg, 8, msg, 8);
	return msg[2];
}

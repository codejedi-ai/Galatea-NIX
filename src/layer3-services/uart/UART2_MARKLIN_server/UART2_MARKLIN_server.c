#include "UART2_MARKLIN_server.h"
#include "io_api.h"
#include "io_common.h"
#include "../../layer1-processes/processes.h"
#include "../../layer1-processes/util.h"
#include "../../layer1-processes/rpi.h"
#include "../../layer1-processes/syscall.h"
#include "../../nameserver.h"

#define DISPLAY 1

void UART2_MARKLIN_server(void)
{
	struct fi_list putc_call_list;
	struct fi_list putc_interrupt_list;
	struct fi_list getc_call_list;
	struct fi_list getc_interrupt_list;
	uint32_t awaitcts[2][NUMPROCS];
	uint32_t awaitcts_size[2];
	uint8_t putc_state;

	memset(&putc_call_list, 0, sizeof(putc_call_list));
	memset(&putc_interrupt_list, 0, sizeof(putc_interrupt_list));
	memset(&getc_call_list, 0, sizeof(getc_call_list));
	memset(&getc_interrupt_list, 0, sizeof(getc_interrupt_list));
	memset(awaitcts, 0, sizeof(awaitcts));
	memset(awaitcts_size, 0, sizeof(awaitcts_size));
	putc_state = 1;

	RegisterAs(UART2_MARKLIN_SERVER);

	while (1)
	{
		int8_t io_notifier_tid = WhoIs("io_notifier");
		int tid;
		char recieve[8];

		Receive(&tid, recieve, 8);
		uint8_t type = recieve[0];
		uint8_t channel = recieve[1];
		uint8_t char_ch = recieve[2];
		uint8_t char_ch2 = recieve[3];

		if (io_notifier_tid == tid)
			Reply(tid, recieve, 0);

		if (type == CTSMIM)
		{
			int cur_cts = get_CTS(MARKLIN);
			for (int i = 0; i < (int)awaitcts_size[cur_cts]; i++)
			{
				int tid_free = (int)awaitcts[cur_cts][i];
				if (tid_free != 0)
				{
					recieve[2] = (char)cur_cts;
					Reply(tid_free, recieve, 8);
				}
			}
			awaitcts_size[cur_cts] = 0;

			if (putc_call_list.call[putc_call_list.begin].tid != 0 &&
			    putc_call_list.size > 0 && char_ch == 1 && putc_state == 3)
			{
				putc_interrupt_list.call[putc_interrupt_list.end].tid = tid;
				putc_interrupt_list.call[putc_interrupt_list.end].type = type;
				putc_interrupt_list.call[putc_interrupt_list.end].channel = channel;
				putc_interrupt_list.call[putc_interrupt_list.end].char_ch = char_ch;
				putc_interrupt_list.call[putc_interrupt_list.end].char_ch2 = char_ch2;
				putc_interrupt_list.end =
					(putc_interrupt_list.end + 1) % QUEUELENGTH;
				putc_interrupt_list.size++;
			}
			if (putc_call_list.call[putc_call_list.begin].tid != 0 &&
			    putc_call_list.size > 0 && char_ch == 0 && putc_state == 2)
				putc_state = 3;
		}

		if (type == TXIC)
		{
			if (putc_call_list.call[putc_call_list.begin].tid != 0 &&
			    putc_call_list.size > 0 && putc_state == 0)
				putc_state = 2;
		}

		if (type == RXIC)
		{
			getc_interrupt_list.call[getc_interrupt_list.end].tid = tid;
			getc_interrupt_list.call[getc_interrupt_list.end].type = type;
			getc_interrupt_list.call[getc_interrupt_list.end].channel = channel;
			getc_interrupt_list.call[getc_interrupt_list.end].char_ch = char_ch;
			getc_interrupt_list.end = (getc_interrupt_list.end + 1) % QUEUELENGTH;
			getc_interrupt_list.size++;
		}
		else if (type == GETC)
		{
			getc_call_list.call[getc_call_list.end].tid = tid;
			getc_call_list.call[getc_call_list.end].type = type;
			getc_call_list.call[getc_call_list.end].channel = channel;
			getc_call_list.call[getc_call_list.end].char_ch = char_ch;
			getc_call_list.end = (getc_call_list.end + 1) % QUEUELENGTH;
			getc_call_list.size++;
		}
		else if (type == PUTC)
		{
			putc_call_list.call[putc_call_list.end].tid = tid;
			putc_call_list.call[putc_call_list.end].type = type;
			putc_call_list.call[putc_call_list.end].channel = channel;
			putc_call_list.call[putc_call_list.end].char_ch = char_ch;
			putc_call_list.call[putc_call_list.end].char_ch2 = char_ch2;
			putc_call_list.end = (putc_call_list.end + 1) % QUEUELENGTH;
			putc_call_list.size++;
		}
		else if (type == CTS)
		{
			if (char_ch == get_CTS(MARKLIN))
				Reply(tid, recieve, 8);
			else
			{
				awaitcts[char_ch][awaitcts_size[char_ch]] = (uint32_t)tid;
				awaitcts_size[char_ch]++;
			}
		}

		if (putc_call_list.call[putc_call_list.begin].tid != 0 &&
		    putc_call_list.size > 0 && putc_state == 1)
		{
			putc_state = 0;
			uart_putc(MARKLIN, putc_call_list.call[putc_call_list.begin].char_ch);
		}

		if (putc_call_list.size > 0 && putc_interrupt_list.size > 0)
		{
			int ret_pid = putc_call_list.call[putc_call_list.begin].tid;
			recieve[0] = putc_call_list.call[putc_call_list.begin].type;
			recieve[1] = putc_call_list.call[putc_call_list.begin].channel;
			recieve[2] = putc_call_list.call[putc_call_list.begin].char_ch;
			recieve[3] = putc_call_list.call[putc_call_list.begin].char_ch2;
			Reply(ret_pid, recieve, 8);
			putc_call_list.call[putc_call_list.begin].tid = 0;
			putc_call_list.begin = (putc_call_list.begin + 1) % QUEUELENGTH;
			putc_call_list.size--;
			putc_interrupt_list.begin =
				(putc_interrupt_list.begin + 1) % QUEUELENGTH;
			putc_interrupt_list.size--;
			putc_state = 1;
		}

		if (getc_call_list.size > 0 && getc_interrupt_list.size > 0)
		{
			int ret_pid = getc_call_list.call[getc_call_list.begin].tid;
			recieve[0] = getc_interrupt_list.call[getc_interrupt_list.begin].type;
			recieve[1] = getc_interrupt_list.call[getc_interrupt_list.begin].channel;
			recieve[2] = getc_interrupt_list.call[getc_interrupt_list.begin].char_ch;
			recieve[3] = getc_interrupt_list.call[getc_interrupt_list.begin].char_ch2;
			Reply(ret_pid, recieve, 8);
			getc_call_list.call[getc_call_list.begin].tid = 0;
			getc_call_list.begin = (getc_call_list.begin + 1) % QUEUELENGTH;
			getc_call_list.size--;
			getc_interrupt_list.begin =
				(getc_interrupt_list.begin + 1) % QUEUELENGTH;
			getc_interrupt_list.size--;
		}
	}

	Exit();
}

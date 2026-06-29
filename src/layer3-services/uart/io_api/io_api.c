#include "io_api.h"
#include "io_common.h"
#include "../../layer1-processes/syscall.h"
#include "../../nameserver.h"

#define DISPLAY 3

int MarklinServerTid(void)
{
	return WhoIs(UART2_MARKLIN_SERVER);
}

int Getc(int tid, int channel)
{
	char channel64[8];

	channel64[0] = GETC;
	channel64[1] = (uint8_t)channel;
	channel64[2] = 0;
	channel64[3] = -1;
	Send(tid, channel64, 8, channel64, 8);
	return channel64[2];
}

int Putc(int tid, int channel, unsigned char ch)
{
	char channel64[8];

	*((uint32_t *)channel64 + 1) = ((uint32_t)channel);
	channel64[0] = PUTC;
	channel64[1] = (uint8_t)channel;
	channel64[2] = ch;
	channel64[3] = -1;
	Send(tid, channel64, 8, channel64, 8);
	#if DISPLAY == 3
	uart_printf(CONSOLE, "Putc: sendret = %d\r\n", channel64[2]);
	#endif
	return channel64[2];
}

int Put2c(int tid, int channel, unsigned char ch, unsigned char ch2)
{
	char channel64[8];

	*((uint32_t *)channel64 + 1) = ((uint32_t)channel);
	channel64[0] = PUTC;
	channel64[1] = (uint8_t)channel;
	channel64[2] = ch;
	channel64[3] = ch2;
	Send(tid, channel64, 8, channel64, 1);
	#if DISPLAY == 2
	uart_printf(CONSOLE, "Put2c: sendret = %d\r\n", channel64[2]);
	#endif
	return channel64[2];
}

int awaitCTS(int tid, int channel, uint8_t val)
{
	char channel64[8];

	#if DISPLAY % 3 == 0
	uart_printf(CONSOLE, "awaitCTS: tid = %u, channel = %u, val = %u\r\n", tid, channel, val);
	#endif
	*((uint32_t *)channel64 + 1) = ((uint32_t)channel);
	channel64[0] = CTS;
	channel64[1] = (uint8_t)channel;
	channel64[2] = val;
	channel64[3] = -1;
	Send(tid, channel64, 8, channel64, 8);
	#if DISPLAY % 3 == 0
	uart_printf(CONSOLE, "awaitCTS: sendret = %d\r\n", channel64[2]);
	#endif
	return channel64[2];
}

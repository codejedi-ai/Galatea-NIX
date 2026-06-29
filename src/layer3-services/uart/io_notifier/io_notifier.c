#include "io_notifier.h"
#include "io_api.h"
#include "UART1_CONSOLE_server.h"
#include "../../layer1-processes/processes.h"
#include "../../layer1-processes/rpi.h"
#include "../../layer1-processes/syscall.h"
#include "../../nameserver.h"

void io_notifier(void)
{
	RegisterAs("io_notifier");
	int uart2_marklin_tid = -1;
	int uart1_console_tid = -1;

	while (1)
	{
		uart2_marklin_tid = WhoIs(UART2_MARKLIN_SERVER);
		uart1_console_tid = WhoIs(UART1_CONSOLE_SERVER);
		uint64_t event = AwaitEvent(UARTINTER);
		int ret;

		uint8_t type = event & 0xFF;
		uint8_t channel = (event >> 8) & 0xFF;

		if (channel == CONSOLE)
		{
			if (uart1_console_tid > 0)
				Send(uart1_console_tid, (const char *)&event, 8, (char *)&ret, 0);
		}
		else if (channel == MARKLIN && uart2_marklin_tid > 0)
		{
			if (type == RXIC || type == TXIC || type == CTSMIM)
				Send(uart2_marklin_tid, (const char *)&event, 8, (char *)&ret, 0);
		}
	}
	Exit();
}

#include "layer2_tests.h"
#include "../messaging.h"
#include "../../layer1-processes/syscall.h"
#include "../../layer1-processes/rpi.h"
#include "../../layer1-processes/shell.h"
#include "../../layer1-processes/project.h"
#include "../../library/string.h"

/*
 * Layer 2: inter-process communication (Send / Receive / Reply).
 *
 * Design rules for this layer (per project direction):
 *   - No hardcoded TIDs: always target the TID that Create() returned, so we
 *     can never collide with a server task whose TID shifts at boot.
 *   - No busy-waiting: synchronization is event-driven. Send() blocks until the
 *     receiver Reply()s and Receive() blocks until a sender arrives; we never
 *     spin on Yield().
 */

/* Receiver side of the handshake: blocks in Receive() until a sender arrives,
 * echoes a fixed reply, then exits. */
static void ipc_echo_child(void)
{
	int from;
	char buf[64];

	int len = Receive(&from, buf, sizeof(buf));
	if (len > 0 && len < (int)sizeof(buf))
		buf[len] = '\0';

	uart_printf(CONSOLE,
		    "    \033[1;34m[ INFO ]\033[0m receiver got \"%s\" from TID %d\r\n",
		    buf, from);

	Reply(from, "world", 6);
	Exit();
}

int run_layer2_tests(void)
{
	int failures = 0;

	uart_printf(CONSOLE, "\r\n");
	uart_printf(CONSOLE, "\033[1;36m[====] %s Layer 2 IPC Tests:\033[0m\r\n",
		    PROJECT_DISPLAY_NAME);

	/* Test #1: Send / Receive / Reply round trip. This task is the SENDER, so
	 * Send() blocks until the receiver replies — no busy-wait — and we target
	 * the TID Create() returned (never a hardcoded number). */
	{
		char reply[64];
		int child = Create(5, ipc_echo_child);
		uart_printf(CONSOLE,
			    "  \033[1;34m[ INFO ]\033[0m Test #1: created receiver TID=%d, sending \"hello\"...\r\n",
			    child);
		int rlen = Send(child, "hello", 6, reply, sizeof(reply));
		if (rlen > 0 && rlen < (int)sizeof(reply))
			reply[rlen] = '\0';
		if (rlen == 6 && strncmp(reply, "world", 6) == 0)
			uart_printf(CONSOLE,
				    "  \033[1;32m[  OK  ]\033[0m Test #1: Send/Receive/Reply round trip (\"hello\" -> \"world\")\r\n");
		else {
			failures++;
			uart_printf(CONSOLE,
				    "  \033[1;31m[ FAIL ]\033[0m Test #1: rlen=%d reply=\"%s\"\r\n",
				    rlen, reply);
		}
	}

	uart_printf(CONSOLE, "\r\n");
	if (failures > 0) {
		uart_printf(CONSOLE,
			    "  \033[1;31m[ FAIL ]\033[0m Layer 2: %d failure(s)\r\n\r\n", failures);
		return -1;
	}
	uart_printf(CONSOLE, "  \033[1;32m[  OK  ]\033[0m All Layer 2 tests passed\r\n");
	uart_printf(CONSOLE, "\r\n");
	return 0;
}

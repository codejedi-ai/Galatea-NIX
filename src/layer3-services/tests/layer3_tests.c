#include "layer3_tests.h"
#include "../clock_client.h"
#include "../clock_messages.h"
#include "../nameserver.h"
#include "../../layer1-processes/syscall.h"
#include "../../layer1-processes/rpi.h"
#include "../../layer1-processes/project.h"

#define TEST_PRINT(fmt, ...) uart_printf(CONSOLE, fmt "\r\n", ##__VA_ARGS__)

void run_layer3_tests(void)
{
	int clock_tid;
	int t0, t1;

	uart_printf(CONSOLE, "\r\n");
	uart_printf(CONSOLE, "\033[1;36m[====] %s Layer 3 Clock Server Tests:\033[0m\r\n",
		    PROJECT_DISPLAY_NAME);

	clock_tid = WhoIs(CLOCK_SERVER_NAME);
	if (clock_tid < 0) {
		TEST_PRINT("  \033[1;31m[ FAIL ]\033[0m WhoIs(\"%s\") failed", CLOCK_SERVER_NAME);
		return;
	}
	TEST_PRINT("  \033[1;32m[  OK  ]\033[0m WhoIs ClockServer TID=%d", clock_tid);

	t0 = Time(clock_tid);
	t1 = Time(clock_tid);
	if (t1 >= t0 && t0 >= 0)
		TEST_PRINT("  \033[1;32m[  OK  ]\033[0m Time(): t0=%d t1=%d (non-decreasing)", t0, t1);
	else
		TEST_PRINT("  \033[1;31m[ FAIL ]\033[0m Time(): t0=%d t1=%d", t0, t1);

	{
		int before = Time(clock_tid);
		int target = before + 3;
		(void)DelayUntil(clock_tid, target);
		int after = Time(clock_tid);
		if (after >= target)
			TEST_PRINT("  \033[1;32m[  OK  ]\033[0m DelayUntil(%d): woke at %d", target, after);
		else
			TEST_PRINT("  \033[1;31m[ FAIL ]\033[0m DelayUntil(%d): after=%d", target, after);
	}

	{
		int before = Time(clock_tid);
		(void)Delay(clock_tid, 2);
		int after = Time(clock_tid);
		if (after >= before + 2)
			TEST_PRINT("  \033[1;32m[  OK  ]\033[0m Delay(2): before=%d after=%d", before, after);
		else
			TEST_PRINT("  \033[1;31m[ FAIL ]\033[0m Delay(2): before=%d after=%d", before, after);
	}

	TEST_PRINT("  \033[1;32m[  OK  ]\033[0m Layer 3 clock tests finished");
	uart_printf(CONSOLE, "\r\n");
}

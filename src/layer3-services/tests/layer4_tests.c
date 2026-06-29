#include "../rps.h"
#include "../rps_messages.h"
#include "../../layer2-messaging/messaging.h"
#include "../../layer1-processes/syscall.h"
#include "../../layer1-processes/rpi.h"
#include "../../layer1-processes/project.h"

#define TEST_PRINT(fmt, ...) uart_printf(CONSOLE, fmt "\r\n", ##__VA_ARGS__)

/* Opponent task: signs up, always plays Scissors, then quits. */
static void rps_opp_scissors(void)
{
	int s = RpsServerTid();
	int opp;
	if (RpsSignup(s) == RPS_R_PAIRED)
		RpsPlay(s, RPS_SCISSORS, &opp);
	RpsQuit(s);
	Exit();
}

/*
 * Layer 4 (services): exercise the K2 Rock-Paper-Scissors server over IPC.
 * We act as player 1 (Rock) against an opponent task (Scissors): Rock wins.
 */
void run_layer4_tests(void)
{
	uart_printf(CONSOLE, "\r\n");
	uart_printf(CONSOLE, "\033[1;36m[====] %s Layer 4 RPS Server Tests:\033[0m\r\n",
		    PROJECT_DISPLAY_NAME);

	int s = RpsServerTid();
	if (s < 0) {
		TEST_PRINT("  \033[1;31m[ FAIL ]\033[0m RPS server not registered");
		return;
	}
	TEST_PRINT("  \033[1;32m[  OK  ]\033[0m RPS server TID=%d", s);

	(void)Create(RPS_CLIENT_PRIORITY, rps_opp_scissors);

	int paired = RpsSignup(s);
	if (paired == RPS_R_PAIRED)
		TEST_PRINT("  \033[1;32m[  OK  ]\033[0m Signup paired two clients");
	else
		TEST_PRINT("  \033[1;31m[ FAIL ]\033[0m Signup result=%d", paired);

	int opp = -1;
	int r = RpsPlay(s, RPS_ROCK, &opp);
	if (r == RPS_R_WIN && opp == RPS_SCISSORS)
		TEST_PRINT("  \033[1;32m[  OK  ]\033[0m Play: Rock beats Scissors (WIN)");
	else
		TEST_PRINT("  \033[1;31m[ FAIL ]\033[0m Play result=%d opp=%d", r, opp);

	RpsQuit(s);
	TEST_PRINT("  \033[1;32m[  OK  ]\033[0m Quit clean; Layer 4 RPS tests finished");
	uart_printf(CONSOLE, "\r\n");
}

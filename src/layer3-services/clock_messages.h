#ifndef CLOCK_MESSAGES_H
#define CLOCK_MESSAGES_H

#define CLOCK_SERVER_NAME "ClockServer"
#define NAME_SERVER_NAME  "NameServer"

/*
 * Service roles (CS452 K3):
 *   Notifier — blocks on AwaitEvent(hardware IRQ), then Send()s to its server.
 *              Only the clock notifier is pinned to CPU 0 (timer IRQ target).
 *   Server   — blocks on Receive(), never touches hardware; Reply()s clients.
 */
typedef enum {
	CLOCK_MSG_TICK = 0,       /* notifier -> clock server only */
	CLOCK_MSG_TIME = 1,
	CLOCK_MSG_DELAY = 2,
	CLOCK_MSG_DELAY_UNTIL = 3,
} ClockMsgType;

typedef struct {
	int type;
	int ticks;
} ClockMsg;

#endif

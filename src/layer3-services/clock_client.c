#include "clock_client.h"
#include "clock_messages.h"
#include "../layer2-messaging/messaging.h"

int Time(int tid)
{
	ClockMsg req;
	int reply = -1;

	req.type = CLOCK_MSG_TIME;
	req.ticks = 0;
	Send(tid, (const char *)&req, (int)sizeof(req), (char *)&reply, (int)sizeof(reply));
	return reply;
}

int Delay(int tid, int ticks)
{
	ClockMsg req;
	int reply = -1;

	req.type = CLOCK_MSG_DELAY;
	req.ticks = ticks;
	Send(tid, (const char *)&req, (int)sizeof(req), (char *)&reply, (int)sizeof(reply));
	return reply;
}

int DelayUntil(int tid, int ticks)
{
	ClockMsg req;
	int reply = -1;

	req.type = CLOCK_MSG_DELAY_UNTIL;
	req.ticks = ticks;
	Send(tid, (const char *)&req, (int)sizeof(req), (char *)&reply, (int)sizeof(reply));
	return reply;
}

#include "display_client.h"
#include "display_tetris.h"
#include "display_messages.h"
#include "displayserver.h"
#include "../layer2-messaging/messaging.h"
#include "../layer1-processes/syscall.h"

static void disp_tetris_send(int type, const DispTetrisState *st)
{
	int tid = DisplayServerTid();
	DispMsg msg;
	int reply = -1;

	if (tid < 0)
		return;
	msg.type  = type;
	msg.len   = 0;
	msg.flags = 0;
	msg.root  = (void *)st;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

void display_tetris_begin(void)
{
	disp_tetris_send(DISP_MSG_TETRIS_BEGIN, 0);
}

void display_tetris_update(const DispTetrisState *st)
{
	if (!st)
		return;
	disp_tetris_send(DISP_MSG_TETRIS_UPDATE, st);
}

void display_tetris_end(void)
{
	disp_tetris_send(DISP_MSG_TETRIS_END, 0);
}

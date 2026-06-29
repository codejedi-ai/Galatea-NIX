#include "display_client.h"
#include "display_messages.h"
#include "displayserver.h"
#include "../layer2-messaging/messaging.h"
#include "../layer2-messaging/console_route.h"
#include "../layer1-processes/config.h"   /* UI_DESIGN_SCREEN_* */
#include "../layer1-processes/rpi.h"
#include "../layer1-processes/syscall.h"
#include "clockserver.h"
#include "clock_client.h"
#include "diskfs.h"

/* ----------------------------------------------------------------- helpers */

static void disp_send_write(const char *data, int len)
{
	int tid = DisplayServerTid();
	DispMsg msg;
	int reply = -1;
	int off = 0;

	if (tid < 0 || !data || len <= 0)
		return;

	while (off < len) {
		int chunk = len - off;
		if (chunk > DISP_MSG_CHUNK) chunk = DISP_MSG_CHUNK;
		msg.type  = DISP_MSG_WRITE;
		msg.len   = chunk;
		msg.flags = 0;
		msg.root  = 0;
		for (int i = 0; i < chunk; i++) msg.data[i] = data[off + i];
		Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
		off += chunk;
	}
}

/* -------------------------------------------------------- legacy ANSI path */

void disp_write(const char *data, int len)
{
	disp_send_write(data, len);
}

void disp_render(void)
{
	int tid = DisplayServerTid();
	DispMsg msg;
	int reply = -1;

	if (tid < 0)
		return;
	msg.type  = DISP_MSG_RENDER;
	msg.len   = 0;
	msg.flags = 0;
	msg.root  = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

void display_clear(void)
{
	int tid = DisplayServerTid();
	DispMsg msg;
	int reply = -1;
	if (tid < 0) return;
	msg.type  = DISP_MSG_CLEAR;
	msg.len   = 0;
	msg.flags = 0;
	msg.root  = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

void display_scroll(int delta)
{
	int tid = DisplayServerTid();
	DispMsg msg;
	int reply = -1;
	if (tid < 0) return;
	msg.type  = DISP_MSG_SCROLL;
	msg.len   = delta;      /* signed line delta: + = older, - = newer */
	msg.flags = 0;
	msg.root  = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

static void disp_console_sink(const char *data, int len)
{
	disp_send_write(data, len);
}

void display_client_register_route(void)
{
	console_route_set(disp_console_sink);
}

/* ------------------------------------------------------- declarative path */

static void disp_send_tree(struct UiElem *root, int flags)
{
	int tid = DisplayServerTid();
	if (tid < 0 || !root) return;
	DispMsg msg;
	int reply = -1;
	msg.type  = DISP_MSG_RENDER_TREE;
	msg.len   = 0;
	msg.flags = flags;
	msg.root  = (void *)root;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

void display_render_tree(struct UiElem *root)
{
	disp_send_tree(root, 0);                 /* incremental: emit only changes  */
}

void display_render_tree_full(struct UiElem *root)
{
	disp_send_tree(root, DISP_FLAG_CLEAR);   /* force a full repaint this frame  */
}

void display_get_size(int *rows, int *cols)
{
	int tid = DisplayServerTid();
	DispSizeReply sz;
	sz.rows = 0;
	sz.cols = 0;
	sz.manual = 0;

	if (tid >= 0) {
		DispMsg msg;
		msg.type  = DISP_MSG_GET_SIZE;
		msg.len   = 0;
		msg.flags = 0;
		msg.root  = 0;
		Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&sz, (int)sizeof(sz));
	}

	if (sz.rows <= 0 || sz.cols <= 0) {
		sz.rows = UI_DESIGN_SCREEN_ROWS;
		sz.cols = UI_DESIGN_SCREEN_COLS;
	}
	if (rows)
		*rows = sz.rows;
	if (cols)
		*cols = sz.cols;
}

void display_query_terminal(int *rows, int *cols)
{
	int tid = DisplayServerTid();
	DispSizeReply sz;
	sz.rows = 0;
	sz.cols = 0;
	sz.manual = 0;

	if (rows)
		*rows = UI_DESIGN_SCREEN_ROWS;
	if (cols)
		*cols = UI_DESIGN_SCREEN_COLS;
	if (!rows || !cols)
		return;

	if (tid >= 0) {
		DispMsg msg;
		msg.type  = DISP_MSG_QUERY_TERMINAL;
		msg.len   = 0;
		msg.flags = 0;
		msg.root  = 0;
		Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&sz, (int)sizeof(sz));
	}

	if (sz.rows > 0 && sz.cols > 0) {
		*rows = sz.rows;
		*cols = sz.cols;
	}
}

void display_set_size(int cols, int rows)
{
	int tid = DisplayServerTid();
	DispSizeReply sz;
	DispMsg msg;

	if (tid < 0)
		return;

	msg.type  = DISP_MSG_SET_SIZE;
	msg.len   = rows;
	msg.flags = cols;
	msg.root  = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&sz, (int)sizeof(sz));
}

void display_screen_auto(void)
{
	int tid = DisplayServerTid();
	DispSizeReply sz;
	DispMsg msg;

	if (tid < 0)
		return;

	msg.type  = DISP_MSG_SCREEN_AUTO;
	msg.len   = 0;
	msg.flags = 0;
	msg.root  = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&sz, (int)sizeof(sz));
}

void display_restore_persisted_screen(void)
{
	char buf[16];
	int n, cols = 0, i = 0;
	int rows;

	n = DiskRead("/screen.cols", buf, (int)sizeof(buf));
	if (n <= 0)
		return;
	while (i < n && buf[i] >= '0' && buf[i] <= '9') {
		cols = cols * 10 + (buf[i] - '0');
		i++;
	}
	if (cols < 16 || cols > 200)
		return;
	rows = (cols * UI_DESIGN_BASE_H) / UI_DESIGN_BASE_W;
	if (rows < 6)
		rows = 6;
	display_set_size(cols, rows);
}

int display_screen_is_manual(void)
{
	int tid = DisplayServerTid();
	DispSizeReply sz;
	DispMsg msg;

	if (tid < 0)
		return 0;

	msg.type  = DISP_MSG_GET_SIZE;
	msg.len   = 0;
	msg.flags = 0;
	msg.root  = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&sz, (int)sizeof(sz));
	return sz.manual ? 1 : 0;
}

void display_write_str(const char *s)
{
	if (!s) return;
	int len = 0;
	while (s[len]) len++;
	if (len > 0)
		disp_write(s, len);
}

void display_put_cell(int row, int col, char ch, const char *attrs)
{
	int tid = DisplayServerTid();
	if (tid < 0 || row < 0 || col < 0)
		return;
	DispMsg msg;
	int reply = -1;
	int ai = 0;

	msg.type  = DISP_MSG_PUT_CELL;
	msg.flags = row;
	msg.len   = col;
	msg.root  = 0;
	msg.data[0] = ch;
	if (attrs) {
		while (attrs[ai] && ai < DISP_MSG_CHUNK - 2) {
			msg.data[ai + 1] = attrs[ai];
			ai++;
		}
	}
	msg.data[ai + 1] = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

void display_put_str(int row, int col, const char *s, const char *attrs)
{
	if (!s)
		return;
	for (int i = 0; s[i]; i++)
		display_put_cell(row, col + i, s[i], attrs);
}

void display_flush(void)
{
	int tid = DisplayServerTid();
	DispMsg msg;
	int reply = -1;

	if (tid < 0)
		return;
	msg.type  = DISP_MSG_FLUSH;
	msg.len   = 0;
	msg.flags = 0;
	msg.root  = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

static int disp_wait_key_fallback(void)
{
	int clock = ClockServerTid();
	while (!uart_rxc(CONSOLE)) {
		if (clock >= 0) Delay(clock, 1);
		else Yield();
	}
	return (int)(unsigned char)uart_getc(CONSOLE);
}

int display_wait_key(void)
{
	int tid = DisplayServerTid();
	int key = -1;

	if (tid >= 0) {
		DispMsg msg;
		msg.type  = DISP_MSG_WAIT_KEY;
		msg.len   = 0;
		msg.flags = 0;
		msg.root  = 0;
		Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&key, (int)sizeof(key));
		if (key >= 0)
			return key;
	}
	return (int)(unsigned char)disp_wait_key_fallback();
}

static void disp_send_cursor(int show)
{
	int tid = DisplayServerTid();
	if (tid < 0) return;
	DispMsg msg;
	int reply = -1;
	msg.type  = DISP_MSG_CURSOR;
	msg.len   = show ? 1 : 0;
	msg.flags = 0;
	msg.root  = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

void display_cursor_show(void)  { disp_send_cursor(1); }
void display_cursor_hide(void)  { disp_send_cursor(0); }

void display_readline_start(void)
{
	int tid = DisplayServerTid();
	if (tid < 0) return;
	DispMsg msg;
	int reply = -1;
	msg.type  = DISP_MSG_READLINE_START;
	msg.len   = 0;
	msg.flags = 0;
	msg.root  = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

void display_shell_region(int top_row, int bottom_row)
{
	int tid = DisplayServerTid();
	if (tid < 0) return;
	DispMsg msg;
	int reply = -1;
	msg.type  = DISP_MSG_SHELL_REGION;
	msg.len   = top_row;
	msg.flags = bottom_row;
	msg.root  = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

void display_erase_char(void)
{
	int tid = DisplayServerTid();
	if (tid < 0) return;
	DispMsg msg;
	int reply = -1;
	msg.type  = DISP_MSG_ERASE_CHAR;
	msg.len   = 0;
	msg.flags = 0;
	msg.root  = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

void display_goto_cursor(int row, int col)
{
	int tid = DisplayServerTid();
	if (tid < 0) return;
	DispMsg msg;
	int reply = -1;
	msg.type  = DISP_MSG_GOTO_CURSOR;
	msg.len   = row;
	msg.flags = col;
	msg.root  = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

void display_get_cursor(int *row, int *col, int *min_col)
{
	int tid = DisplayServerTid();
	DispCursorReply cr;
	cr.row = 0;
	cr.col = 0;
	cr.min_col = 0;
	if (tid >= 0) {
		DispMsg msg;
		msg.type  = DISP_MSG_GET_CURSOR;
		msg.len   = 0;
		msg.flags = 0;
		msg.root  = 0;
		Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&cr, (int)sizeof(cr));
	}
	if (row)     *row     = cr.row;
	if (col)     *col     = cr.col;
	if (min_col) *min_col = cr.min_col;
}

void display_readline_echo(char c)
{
	int tid = DisplayServerTid();
	if (tid < 0) return;
	DispMsg msg;
	int reply = -1;
	msg.type    = DISP_MSG_READLINE_ECHO;
	msg.len     = 0;
	msg.flags   = 0;
	msg.root    = 0;
	msg.data[0] = c;
	msg.data[1] = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

void display_readline_end(void)
{
	int tid = DisplayServerTid();
	if (tid < 0) return;
	DispMsg msg;
	int reply = -1;
	msg.type  = DISP_MSG_READLINE_END;
	msg.len   = 0;
	msg.flags = 0;
	msg.root  = 0;
	Send(tid, (const char *)&msg, (int)sizeof(msg), (char *)&reply, (int)sizeof(reply));
}

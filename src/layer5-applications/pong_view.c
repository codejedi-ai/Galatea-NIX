#include "pong_view.h"
#include "apps.h"
#include "clock_client.h"
#include "clockserver.h"
#include "ui_elem.h"
#include "ui_app.h"
#include "../layer3-services/display_client.h"
#include "../layer1-processes/config.h"

static void pong_view_status(const PongModel *m, char *st, int cap)
{
	char num[16]; int p = 0;
	app_append(st, &p, "P1:");
	app_uitoa((unsigned)m->p1_score, num);
	app_append(st, &p, num);
	app_append(st, &p, " P2:");
	app_uitoa((unsigned)m->p2_score, num);
	app_append(st, &p, num);
	app_append(st, &p, "  P1=W/S  P2=Up/Dn  p=pause");
	if (p >= cap) st[cap - 1] = 0;
	else st[p] = 0;
}

static void pong_view_draw_state(PongView *v, const PongModel *m)
{
	int w = v->cv.w, h = v->cv.h;
	ui_canvas_clear(&v->cv);

	for (int i = 0; i < PONG_PAD; i++) {
		if (m->p1_y + i < h) ui_canvas_put_cell(&v->cv, m->p1_y + i, 0, '#');
		if (m->p2_y + i < h) ui_canvas_put_cell(&v->cv, m->p2_y + i, w - 1, '#');
	}
	if (m->ball_x >= 0 && m->ball_x < w && m->ball_y >= 0 && m->ball_y < h)
		ui_canvas_put_cell(&v->cv, m->ball_y, m->ball_x, '.');
}

static void pong_view_send_tree(PongView *v, const PongModel *m, int full)
{
	char st[80];
	int rows, cols;
	UiAppShell sh;
	UiElem canvas_elem = {
		.type  = UI_CANVAS,
		.cells = (const char (*)[UI_CANVAS_WMAX])v->cv.cells,
		.cw    = v->cv.w,
		.ch    = v->cv.h
	};
	UiElem content[1] = { canvas_elem };

	pong_view_status(m, st, (int)sizeof(st));
	display_get_size(&rows, &cols);
	ui_app_shell_init(&sh, "Pong 2P", st, rows, content, 1);
	ui_app_shell_render(&sh, full);
}

void pong_view_reset(PongView *v, const PongModel *m)
{
	v->t0 = Time(ClockServerTid());
	pong_view_draw_state(v, m);
	display_clear();
	pong_view_send_tree(v, m, 1);
}

void pong_view_render(PongView *v, const PongModel *m)
{
	pong_view_draw_state(v, m);
	pong_view_send_tree(v, m, 0);
}

void pong_view_play_rect(const PongView *v, int *row, int *col, int *width)
{
	/* meter row + DIV top chrome (border + title + sep), canvas vertical centre */
	*row   = ui_app_content_center_row(v->cv.h);
	*col   = 2;
	*width = v->cv.w;
}

#include "apps.h"
#include "game.h"
#include "layer4_ui.h"
#include "ui_elem.h"
#include "ui_app.h"
#include "clock_client.h"
#include "clockserver.h"
#include "../layer3-services/display_client.h"
#include "../layer1-processes/config.h"
#include "../layer1-processes/rpi.h"

/*
 * Snake — textbook MVC, rendered through the display server's declarative
 * UiElem path (4 meter bars + a bordered canvas), so the server diffs frames
 * and repaints only the cells that move.
 *
 * The body is modelled as a QUEUE: each move enqueues the new head and (unless
 * food was eaten) dequeues the tail — O(1) per tick instead of shifting an
 * array. A parallel occupancy grid makes self-collision and food placement O(1)
 * too. Head of the snake = newest element; tail = oldest.
 */

#define SNK_WMAX UI_CANVAS_WMAX
#define SNK_HMAX UI_CANVAS_HMAX
#define SNK_CAP  (SNK_WMAX * SNK_HMAX)   /* queue capacity = board cell count */

/* ───────────────────────────── Model ──────────────────────────────────── */

typedef struct { short x, y; } Seg;

static Seg          q[SNK_CAP];                 /* circular queue of segments */
static int          q_head;                     /* index of the tail segment  */
static int          q_count;                    /* live segments = snake len   */
static unsigned char occ[SNK_HMAX][SNK_WMAX];   /* 1 = a segment occupies cell */

static int FW, FH;                              /* board size (cells)          */
static int s_dx, s_dy;                          /* current heading             */
static int s_score, s_fx, s_fy, s_over;

/* turn buffer: queue rapid key presses so a quick double-turn isn't lost */
#define IN_Q 16
static int in_qx[IN_Q], in_qy[IN_Q];
static int in_head, in_tail;
static int qd_dx, qd_dy;                        /* heading after last queued turn */

static Seg snake_head(void) { return q[(q_head + q_count - 1) % SNK_CAP]; }

static void enqueue_head(int x, int y)
{
	q[(q_head + q_count) % SNK_CAP] = (Seg){ (short)x, (short)y };
	q_count++;
	occ[y][x] = 1;
}

static void dequeue_tail(void)
{
	Seg t = q[q_head];
	occ[t.y][t.x] = 0;
	q_head = (q_head + 1) % SNK_CAP;
	q_count--;
}

static void place_food(void)
{
	for (int tries = 0; tries < 400; tries++) {
		int x = (int)(ui_rng() % (unsigned)FW);
		int y = (int)(ui_rng() % (unsigned)FH);
		if (!occ[y][x]) { s_fx = x; s_fy = y; return; }
	}
	/* board nearly full: linear scan for any free cell */
	for (int y = 0; y < FH; y++)
		for (int x = 0; x < FW; x++)
			if (!occ[y][x]) { s_fx = x; s_fy = y; return; }
	s_fx = -1; s_fy = -1;   /* no free cell: park food off-board (not drawn) */
}

static void model_init(int w, int h)
{
	FW = w; FH = h;
	q_head = 0; q_count = 0;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			occ[y][x] = 0;

	s_dx = 1; s_dy = 0; s_score = 0; s_over = 0;
	in_head = in_tail = 0;
	qd_dx = s_dx; qd_dy = s_dy;

	int cx = w / 2, cy = h / 2;
	for (int i = 3; i >= 0; i--) enqueue_head(cx - i, cy);  /* tail→head */
	place_food();
}

static void model_tick(void)
{
	/* apply at most one buffered turn this frame */
	if (in_head != in_tail) {
		s_dx = in_qx[in_head]; s_dy = in_qy[in_head];
		in_head = (in_head + 1) % IN_Q;
	}

	Seg hd = snake_head();
	int nx = hd.x + s_dx, ny = hd.y + s_dy;

	if (nx < 0 || nx >= FW || ny < 0 || ny >= FH) { s_over = 1; return; }

	int ate = (nx == s_fx && ny == s_fy);

	/* hitting the body is fatal — except the tail cell, which vacates this
	 * tick (so long as we are not growing into it). */
	if (occ[ny][nx]) {
		Seg tail = q[q_head];
		if (!(nx == tail.x && ny == tail.y && !ate)) { s_over = 1; return; }
	}

	if (!ate) dequeue_tail();
	enqueue_head(nx, ny);
	if (ate) {
		s_score++;
		if (q_count >= FW * FH) { s_over = 1; return; }  /* board solved — you win */
		place_food();
	}
}

/* ───────────────────────────── View ───────────────────────────────────── */

static UiCanvas g_cv;

static void view_status(char *st, int cap)
{
	char num[16]; int p = 0;
	app_append(st, &p, "Score: ");
	app_uitoa((unsigned)s_score, num);
	app_append(st, &p, num);
	app_append(st, &p, "   WASD / arrows move   p pause");
	if (p >= cap) st[cap - 1] = 0; else st[p] = 0;
}

static void view_draw(void)
{
	ui_canvas_clear(&g_cv);
	for (int i = 0; i < q_count; i++) {
		int idx = (q_head + i) % SNK_CAP;
		ui_canvas_put_cell(&g_cv, q[idx].y, q[idx].x,
		                   (i == q_count - 1) ? 'O' : 'o');   /* head 'O', body 'o' */
	}
	if (s_fx >= 0) ui_canvas_put_cell(&g_cv, s_fy, s_fx, '@');
}

static void view_send(int full)
{
	char st[80];
	int rows, cols;
	UiAppShell sh;
	UiElem canvas = { .type = UI_CANVAS,
	                  .cells = (const char (*)[UI_CANVAS_WMAX])g_cv.cells,
	                  .cw = g_cv.w, .ch = g_cv.h };
	UiElem content[1] = { canvas };

	view_status(st, (int)sizeof(st));
	display_get_size(&rows, &cols);
	ui_app_shell_init(&sh, "SNAKE", st, rows, content, 1);
	ui_app_shell_render(&sh, full);
}

/* ─────────────────────────── Controller ───────────────────────────────── */

static void snake_reset(void)
{
	int w, h;

	ui_app_canvas_fill_body(&w, &h);

	ui_canvas_init(&g_cv);
	g_cv.w = w; g_cv.h = h;

	model_init(w, h);
	view_draw();
	display_clear();
	view_send(1);
}

static void snake_key(int k)
{
	int ndx = qd_dx, ndy = qd_dy;
	if      (k == 'w' || k == 'W' || k == UI_KEY_UP)    { ndx = 0;  ndy = -1; }
	else if (k == 's' || k == 'S' || k == UI_KEY_DOWN)  { ndx = 0;  ndy =  1; }
	else if (k == 'a' || k == 'A' || k == UI_KEY_LEFT)  { ndx = -1; ndy =  0; }
	else if (k == 'd' || k == 'D' || k == UI_KEY_RIGHT) { ndx =  1; ndy =  0; }
	else return;

	if (ndx == -qd_dx && ndy == -qd_dy) return;  /* no 180° reversal */
	if (ndx ==  qd_dx && ndy ==  qd_dy) return;  /* already heading that way */

	int nt = (in_tail + 1) % IN_Q;
	if (nt == in_head) return;                    /* turn buffer full */
	in_qx[in_tail] = ndx; in_qy[in_tail] = ndy;
	in_tail = nt;
	qd_dx = ndx; qd_dy = ndy;
}

static int snake_tick(void)
{
	model_tick();
	if (s_over) return GAME_OVER;
	view_draw();
	view_send(0);
	return GAME_CONTINUE;
}

static void snake_rect(int *row, int *col, int *width)
{
	*row   = ui_app_content_center_row(g_cv.h);
	*col   = 2;
	*width = g_cv.w;
}

void app_snake(void)
{
	static const Game g = { .frame_ms = 6, .reset = snake_reset, .key = snake_key,
	                        .tick = snake_tick, .rect = snake_rect };
	game_run(&g);
}

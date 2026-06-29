#include "display_tetris.h"
#include "../layer4-ui/screenbuf.h"
#include "../layer4-ui/ui_elem.h"
#include "../layer4-ui/ui_app.h"
#include "../layer4-ui/ui_screen.h"
#include "apu_client.h"
#include "../layer1-processes/config.h"

/*
 * Tetris board rendering on APU cores 1–3 only.
 * CPU0 dispatches bands and drains UART; no pixel work on CPU0.
 */

extern ScreenBuf g_sb;

static DispTetrisState g_tetris;
static int g_tetris_on;

static const unsigned char TETRIS_FG[7] = { 96, 93, 95, 92, 91, 94, 33 };

static const signed char TETRIS_PIECE[7][4][2] = {
	{ {0,1},{1,1},{2,1},{3,1} },
	{ {1,0},{2,0},{1,1},{2,1} },
	{ {1,0},{0,1},{1,1},{2,1} },
	{ {1,0},{2,0},{0,1},{1,1} },
	{ {0,0},{1,0},{1,1},{2,1} },
	{ {0,0},{0,1},{1,1},{2,1} },
	{ {2,0},{0,1},{1,1},{2,1} },
};

#define TETRIS_CELL      DISP_TETRIS_CELL
#define TETRIS_PANEL_NEXT_ROW    2
#define TETRIS_PANEL_PREVIEW_ROW (TETRIS_PANEL_NEXT_ROW + 2)

static int g_well_row;
static int g_well_col;
static int g_box_row;
static int g_box_col;
static int g_cluster_row;
static int g_left_col;
static int g_right_col;

static int tetris_inner_w(void)
{
	return g_sb.cols > 2 ? g_sb.cols - 2 : g_sb.cols;
}

static int tetris_body_h(void)
{
	return ui_screen_body_h(g_sb.rows);
}

static void tetris_calc_layout(void)
{
	int inner = tetris_inner_w();
	int body_h = tetris_body_h();
	int start = 1 + (inner - DISP_TETRIS_CLUSTER_W) / 2;

	if (start < 1)
		start = 1;
	g_cluster_row = UI_APP_CONTENT_ROW + (body_h - DISP_TETRIS_BOX_H) / 2;
	if (g_cluster_row < UI_APP_CONTENT_ROW)
		g_cluster_row = UI_APP_CONTENT_ROW;
	g_left_col  = start;
	g_box_col   = start + DISP_TETRIS_INSTR_W + DISP_TETRIS_SIDE_GAP;
	g_well_col  = g_box_col + 1;
	g_box_row   = g_cluster_row;
	g_well_row  = g_box_row + 1;
	g_right_col = g_box_col + DISP_TETRIS_BOX_W + DISP_TETRIS_SIDE_GAP;
}

static void tetris_attr_fg(unsigned char code, unsigned char a[SBUF_ATTRS])
{
	int p = 0;

	if (!code) {
		a[0] = 0;
		return;
	}
	a[p++] = '\033';
	a[p++] = '[';
	a[p++] = '0';
	a[p++] = ';';
	if (code >= 10)
		a[p++] = (unsigned char)('0' + code / 10);
	a[p++] = (unsigned char)('0' + (code % 10));
	a[p++] = 'm';
	a[p] = 0;
}

static void tetris_put(ScreenBuf *sb, int row, int col, char ch, unsigned char fg)
{
	unsigned char g[3];
	unsigned char a[SBUF_ATTRS];

	g[0] = (unsigned char)ch;
	g[1] = 0;
	g[2] = 0;
	tetris_attr_fg(fg, a);
	screenbuf_set(sb, row, col, g, a);
}

static void tetris_put_utf8(ScreenBuf *sb, int row, int col,
                            unsigned char b0, unsigned char b1, unsigned char b2,
                            unsigned char fg)
{
	unsigned char g[3];
	unsigned char a[SBUF_ATTRS];

	g[0] = b0;
	g[1] = b1;
	g[2] = b2;
	tetris_attr_fg(fg, a);
	screenbuf_set(sb, row, col, g, a);
}

static void tetris_put_str(ScreenBuf *sb, int row, int col, const char *s,
                           unsigned char fg)
{
	int i;

	for (i = 0; s[i]; i++)
		tetris_put(sb, row, col + i, s[i], fg);
}

static void tetris_append(char *st, int *p, const char *s)
{
	while (*s && *p < 120) st[(*p)++] = *s++;
}

static void tetris_uitoa(unsigned v, char *num)
{
	int i = 0;
	char t[12];
	int j = 0;

	if (v == 0) {
		num[0] = '0';
		num[1] = 0;
		return;
	}
	while (v) {
		t[i++] = (char)('0' + v % 10);
		v /= 10;
	}
	while (i)
		num[j++] = t[--i];
	num[j] = 0;
}

/*
 * Pick the glyph for one cell of an N×N ASCII block (dr/dc are 0-based).
 *
 *   N=1: █       N=2: ┌┐     N=3: ┌──┐     N=4: ┌───┐
 *                    └┘          │  │          │   │
 *                                 └──┘          │   │
 *                                               └───┘
 *
 * Returns 1 if UTF-8 glyph (*b0..*b2), 0 if ASCII space.
 */
static int tetris_block_glyph(int dr, int dc, int n,
                              unsigned char *b0, unsigned char *b1, unsigned char *b2)
{
	int last = n - 1;

	if (n <= 0)
		return 0;
	if (n == 1) {
		/* U+2588 FULL BLOCK █ */
		*b0 = 0xE2; *b1 = 0x96; *b2 = 0x88;
		return 1;
	}
	if (dr == 0 && dc == 0) {
		*b0 = 0xE2; *b1 = 0x94; *b2 = 0x8C;
		return 1;
	}
	if (dr == 0 && dc == last) {
		*b0 = 0xE2; *b1 = 0x94; *b2 = 0x90;
		return 1;
	}
	if (dr == last && dc == 0) {
		*b0 = 0xE2; *b1 = 0x94; *b2 = 0x94;
		return 1;
	}
	if (dr == last && dc == last) {
		*b0 = 0xE2; *b1 = 0x94; *b2 = 0x98;
		return 1;
	}
	if (dr == 0 || dr == last) {
		*b0 = 0xE2; *b1 = 0x94; *b2 = 0x80;
		return 1;
	}
	if (dc == 0 || dc == last) {
		*b0 = 0xE2; *b1 = 0x94; *b2 = 0x82;
		return 1;
	}
	return 0;
}

static void tetris_put_glyph(ScreenBuf *sb, int row, int col, unsigned char fg,
                             int dr, int dc, int n)
{
	unsigned char b0, b1, b2;

	if (tetris_block_glyph(dr, dc, n, &b0, &b1, &b2))
		tetris_put_utf8(sb, row, col, b0, b1, b2, fg);
	else
		tetris_put(sb, row, col, ' ', 0);
}

static void tetris_empty_at(ScreenBuf *sb, int sr, int sc)
{
	int dr, dc;
	int n = TETRIS_CELL;

	for (dr = 0; dr < n; dr++)
		for (dc = 0; dc < n; dc++)
			tetris_put(sb, sr + dr, sc + dc, ' ', 0);
}

static void tetris_draw_at(ScreenBuf *sb, int sr, int sc, unsigned char fg)
{
	int dr, dc;
	int n = TETRIS_CELL;

	for (dr = 0; dr < n; dr++)
		for (dc = 0; dc < n; dc++)
			tetris_put_glyph(sb, sr + dr, sc + dc, fg, dr, dc, n);
}

static void tetris_empty_block(ScreenBuf *sb, int br, int bc)
{
	int sr = g_well_row + br * TETRIS_CELL;
	int sc = g_well_col + bc * TETRIS_CELL;

	tetris_empty_at(sb, sr, sc);
}

static void tetris_draw_block(ScreenBuf *sb, int br, int bc, unsigned char fg)
{
	int sr = g_well_row + br * TETRIS_CELL;
	int sc = g_well_col + bc * TETRIS_CELL;

	tetris_draw_at(sb, sr, sc, fg);
}

static int tetris_cell(const DispTetrisState *st, int br, int bc)
{
	int i;

	if (st->active) {
		for (i = 0; i < 4; i++)
			if (st->cury[i] == br && st->curx[i] == bc)
				return st->curtype + 1;
	}
	return (unsigned char)st->board[br][bc];
}

typedef struct {
	ScreenBuf          *sb;
	const DispTetrisState *st;
	int                 row0;
	int                 br_start;
	int                 br_end;
	int                 paint_panel;
} TetrisBand;

static TetrisBand g_tbands[3];

static void tetris_panel_cell(ScreenBuf *sb, const DispTetrisState *st, int pr, int pc)
{
	int on = 0;
	int i;
	int sr = g_cluster_row + TETRIS_PANEL_PREVIEW_ROW + pr * TETRIS_CELL;
	int sc = g_right_col + pc * TETRIS_CELL;

	for (i = 0; i < 4; i++)
		if (TETRIS_PIECE[st->nexttype][i][0] == pc
		    && TETRIS_PIECE[st->nexttype][i][1] == pr)
			on = 1;
	if (on)
		tetris_draw_at(sb, sr, sc, TETRIS_FG[st->nexttype]);
	else
		tetris_empty_at(sb, sr, sc);
}

static void tetris_paint_instructions(ScreenBuf *sb)
{
	static const char *const lines[] = {
		"CONTROLS",
		"a/d  move",
		"w    rotate",
		"s    soft drop",
		"spc  hard drop",
		"p    pause",
		0
	};
	int row = g_cluster_row;
	int i;

	for (i = 0; lines[i]; i++, row++)
		tetris_put_str(sb, row, g_left_col, lines[i], 0);
}

static void tetris_paint_right_panel(ScreenBuf *sb, const DispTetrisState *st)
{
	char num[16];
	int br, bc;

	tetris_put_str(sb, g_cluster_row, g_right_col, "LEVEL", 0);
	tetris_uitoa(st->level, num);
	tetris_put_str(sb, g_cluster_row, g_right_col + 6, num, 0);
	tetris_put_str(sb, g_cluster_row + TETRIS_PANEL_NEXT_ROW, g_right_col, "NEXT", 0);
	for (br = 0; br < DISP_TETRIS_PANEL_CELLS; br++)
		for (bc = 0; bc < DISP_TETRIS_PANEL_CELLS; bc++)
			tetris_panel_cell(sb, st, br, bc);
}

static void tetris_paint_well_box(ScreenBuf *sb)
{
	int r, c;
	int r0 = g_box_row;
	int c0 = g_box_col;
	int r1 = r0 + DISP_TETRIS_BOX_H - 1;
	int c1 = c0 + DISP_TETRIS_BOX_W - 1;

	tetris_put_utf8(sb, r0, c0, 0xE2, 0x94, 0x8C, 0);
	for (c = 1; c <= DISP_TETRIS_DISP_W; c++)
		tetris_put_utf8(sb, r0, c0 + c, 0xE2, 0x94, 0x80, 0);
	tetris_put_utf8(sb, r0, c1, 0xE2, 0x94, 0x90, 0);
	for (r = 1; r <= DISP_TETRIS_DISP_H; r++) {
		tetris_put_utf8(sb, r0 + r, c0, 0xE2, 0x94, 0x82, 0);
		tetris_put_utf8(sb, r0 + r, c1, 0xE2, 0x94, 0x82, 0);
	}
	tetris_put_utf8(sb, r1, c0, 0xE2, 0x94, 0x94, 0);
	for (c = 1; c <= DISP_TETRIS_DISP_W; c++)
		tetris_put_utf8(sb, r1, c0 + c, 0xE2, 0x94, 0x80, 0);
	tetris_put_utf8(sb, r1, c1, 0xE2, 0x94, 0x98, 0);
}

static void tetris_band_fn(void *arg)
{
	TetrisBand *b = arg;
	int br, bc;

	for (br = b->br_start; br < b->br_end; br++) {
		for (bc = 0; bc < DISP_TETRIS_BW; bc++) {
			int v = tetris_cell(b->st, br, bc);

			if (v)
				tetris_draw_block(b->sb, br, bc, TETRIS_FG[v - 1]);
			else
				tetris_empty_block(b->sb, br, bc);
		}
	}
}

static int tetris_status_row(void)
{
	return UI_APP_CONTENT_ROW + tetris_body_h() + 1;
}

static void tetris_format_status(const DispTetrisState *st, char *stout, int cap)
{
	char num[16];
	int p = 0;

	tetris_append(stout, &p, "SCORE ");
	tetris_uitoa(st->score, num);
	tetris_append(stout, &p, num);
	tetris_append(stout, &p, "  LINES ");
	tetris_uitoa(st->lines, num);
	tetris_append(stout, &p, num);
	if (p >= cap)
		stout[cap - 1] = 0;
	else
		stout[p] = 0;
}

void disp_tetris_render_shell(const char *status);

static void tetris_paint_board(void)
{
	int band = (DISP_TETRIS_BH + 2) / 3;
	int i;

	tetris_calc_layout();
	screenbuf_copy_rect(&g_sb, UI_APP_CONTENT_ROW, 1,
	                    tetris_inner_w(), tetris_body_h());

	ApuJob jobs[3];
	int nj = 0;
	for (i = 0; i < 3; i++) {
		int br0 = i * band;
		int br1 = br0 + band;
		if (br1 > DISP_TETRIS_BH)
			br1 = DISP_TETRIS_BH;
		g_tbands[i].sb         = &g_sb;
		g_tbands[i].st         = &g_tetris;
		g_tbands[i].row0       = g_well_row;
		g_tbands[i].br_start   = br0;
		g_tbands[i].br_end     = br1;
		g_tbands[i].paint_panel = 0;
		if (br0 < br1) {
			jobs[nj].fn  = tetris_band_fn;
			jobs[nj].arg = &g_tbands[i];
			nj++;
		}
	}
	if (nj > 0)
		APUBatch(jobs, nj);
	tetris_paint_well_box(&g_sb);
	tetris_paint_instructions(&g_sb);
	tetris_paint_right_panel(&g_sb, &g_tetris);
}

static void tetris_paint_status(void)
{
	char st[128];
	int row = tetris_status_row();
	int inner = tetris_inner_w();

	tetris_format_status(&g_tetris, st, (int)sizeof(st));
	if (row < g_sb.rows) {
		unsigned char a[1] = { 0 };
		screenbuf_set(&g_sb, row, 0, (const unsigned char *)" ", a);
		for (int c = 1; c < 1 + inner && c < g_sb.cols - 1; c++)
			screenbuf_set(&g_sb, row, c, (const unsigned char *)" ", a);
		for (int i = 0; st[i] && 1 + i < inner; i++)
			tetris_put(&g_sb, row, 1 + i, st[i], 0);
	}
}

void disp_tetris_begin(void)
{
	char st[128];
	DispTetrisState z;

	for (int r = 0; r < DISP_TETRIS_BH; r++)
		for (int c = 0; c < DISP_TETRIS_BW; c++)
			z.board[r][c] = 0;
	z.curtype = 0;
	z.nexttype = 0;
	z.active = 0;
	z.score = z.lines = z.level = 0;
	g_tetris = z;
	g_tetris_on = 1;
	tetris_format_status(&g_tetris, st, (int)sizeof(st));
	disp_tetris_render_shell(st);
}

void disp_tetris_update(const DispTetrisState *st)
{
	if (!st || !g_tetris_on)
		return;
	g_tetris = *st;
	tetris_paint_board();
	tetris_paint_status();
}

void disp_tetris_end(void)
{
	g_tetris_on = 0;
}

int disp_tetris_active(void)
{
	return g_tetris_on;
}

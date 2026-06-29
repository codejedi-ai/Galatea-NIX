#include "apps.h"
#include "game.h"
#include "layer4_ui.h"
#include "../layer4-ui/ui_app.h"
#include "clock_client.h"
#include "clockserver.h"
#include "../layer3-services/display_client.h"
#include "../layer3-services/display_tetris.h"
#include "../layer1-processes/rpi.h"

/*
 * Tetris model — board[20][10] holds grounded blocks only.
 * Active piece = curx/cury + curtype (not in board[][]).
 * View: display server renders from DispTetrisState on APU cores 1–3.
 */
#define BW  DISP_TETRIS_BW
#define BH  DISP_TETRIS_BH
/* Hidden rows above row 0 — pieces enter from here; game over only when
 * the spawn overlaps grounded blocks inside the well (y >= 0). */
#define TETRIS_SPAWN_Y  (-2)

/* Game loop: Delay(clock, TETRIS_LOOP_TICKS); clock server tick = 10 ms. */
#define TETRIS_LOOP_TICKS  3
#define CLOCK_MS           10

/*
 * NES Tetris gravity — frames per drop at 60 Hz (level 0 .. 13+).
 * Our t_level is 1-based (t_level 1 → NES level 0 → 48 frames ≈ 800 ms).
 */
static const unsigned char NES_DROP_FRAMES[] = {
	48, 43, 38, 33, 28, 23, 18, 13, 8, 6, 5, 4, 3, 2
};
#define NES_DROP_LEVELS ((int)(sizeof(NES_DROP_FRAMES) / sizeof(NES_DROP_FRAMES[0])))

static char board[BH][BW];
static int curx[4], cury[4], curtype, nexttype;
static unsigned t_score, t_lines, t_level;
static int t_gcd, t_hard;
static int g_pending_over;
static int g_dirty;

static const signed char PIECE[7][4][2] = {
	{ {0,1},{1,1},{2,1},{3,1} },
	{ {1,0},{2,0},{1,1},{2,1} },
	{ {1,0},{0,1},{1,1},{2,1} },
	{ {1,0},{2,0},{0,1},{1,1} },
	{ {0,0},{1,0},{1,1},{2,1} },
	{ {0,0},{0,1},{1,1},{2,1} },
	{ {2,0},{0,1},{1,1},{2,1} },
};

static void spawn(int type, int *ox, int *oy)
{
	for (int i = 0; i < 4; i++) {
		ox[i] = BW / 2 - 2 + PIECE[type][i][0];
		oy[i] = PIECE[type][i][1] + TETRIS_SPAWN_Y;
	}
}

static int collide(const int *ox, const int *oy)
{
	for (int i = 0; i < 4; i++) {
		int x = ox[i], y = oy[i];
		if (x < 0 || x >= BW) return 1;
		if (y >= BH) return 1;
		if (y >= 0 && board[y][x]) return 1;
	}
	return 0;
}

/* Game over: new piece overlaps the stack inside the well (not hidden rows). */
static int spawn_blocked(void)
{
	for (int i = 0; i < 4; i++) {
		int x = curx[i], y = cury[i];
		if (x < 0 || x >= BW || y >= BH) return 1;
		if (y >= 0 && board[y][x]) return 1;
	}
	return 0;
}

static int try_set(const int *nx, const int *ny)
{
	if (collide(nx, ny))
		return 0;
	for (int i = 0; i < 4; i++) {
		curx[i] = nx[i];
		cury[i] = ny[i];
	}
	return 1;
}

static int gravity_spd(void)
{
	int idx = (int)t_level - 1;
	unsigned nes_ms;
	unsigned loop_ms;
	int spd;

	if (idx < 0)
		idx = 0;
	if (idx >= NES_DROP_LEVELS)
		idx = NES_DROP_LEVELS - 1;

	nes_ms  = (unsigned)NES_DROP_FRAMES[idx] * 1000u / 60u;
	loop_ms = (unsigned)TETRIS_LOOP_TICKS * (unsigned)CLOCK_MS;
	spd     = (int)((nes_ms + loop_ms / 2u) / loop_ms);
	if (spd < 1)
		spd = 1;
	return spd;
}

static void gravity_arm(void)
{
	t_gcd = gravity_spd();
}

static void active_piece_spawn(int type)
{
	spawn(type, curx, cury);
	gravity_arm();
}

static void tetris_push_state(void)
{
	DispTetrisState st;
	int r, c;

	for (r = 0; r < BH; r++)
		for (c = 0; c < BW; c++)
			st.board[r][c] = board[r][c];
	for (int i = 0; i < 4; i++) {
		st.curx[i] = curx[i];
		st.cury[i] = cury[i];
	}
	st.curtype  = (signed char)curtype;
	st.nexttype = (signed char)nexttype;
	st.active   = 1;
	st.score    = t_score;
	st.lines    = t_lines;
	st.level    = t_level;
	display_tetris_update(&st);
	g_dirty = 0;
}

static int piece_lock(void)
{
	for (int i = 0; i < 4; i++)
		if (cury[i] >= 0)
			board[cury[i]][curx[i]] = (char)(curtype + 1);

	int cleared = 0;
	for (int r = BH - 1; r >= 0; r--) {
		int full = 1;
		for (int cc = 0; cc < BW; cc++)
			if (!board[r][cc]) { full = 0; break; }
		if (full) {
			cleared++;
			for (int rr = r; rr > 0; rr--)
				for (int cc = 0; cc < BW; cc++)
					board[rr][cc] = board[rr - 1][cc];
			for (int cc = 0; cc < BW; cc++) board[0][cc] = 0;
			r++;
		}
	}
	if (cleared) {
		static const unsigned pts[5] = { 0, 100, 300, 500, 800 };
		t_score += pts[cleared] * t_level;
		t_lines += (unsigned)cleared;
		t_level = 1 + t_lines / 10;
	}

	curtype = nexttype;
	nexttype = (int)(ui_rng() % 7);
	active_piece_spawn(curtype);
	g_dirty = 1;
	return spawn_blocked();
}

static void piece_hard_drop(void)
{
	int nx[4], ny[4];

	for (;;) {
		for (int i = 0; i < 4; i++) {
			nx[i] = curx[i];
			ny[i] = cury[i] + 1;
		}
		if (collide(nx, ny)) break;
		for (int i = 0; i < 4; i++) cury[i]++;
		t_score += 2;
	}
	g_dirty = 1;
}

static void tetris_reset(void)
{
	t_score = 0; t_lines = 0; t_level = 1; t_hard = 0;
	g_pending_over = 0;
	g_dirty = 0;
	for (int r = 0; r < BH; r++) for (int c = 0; c < BW; c++) board[r][c] = 0;
	curtype  = (int)(ui_rng() % 7);
	nexttype = (int)(ui_rng() % 7);
	active_piece_spawn(curtype);
	display_tetris_begin();
	g_dirty = 1;
	tetris_push_state();
}

static void tetris_key(int c)
{
	int nx[4], ny[4];

	if (c == 'a' || c == 'A' || c == UI_KEY_LEFT) {
		for (int i = 0; i < 4; i++) { nx[i] = curx[i] - 1; ny[i] = cury[i]; }
		if (try_set(nx, ny)) g_dirty = 1;
	} else if (c == 'd' || c == 'D' || c == UI_KEY_RIGHT) {
		for (int i = 0; i < 4; i++) { nx[i] = curx[i] + 1; ny[i] = cury[i]; }
		if (try_set(nx, ny)) g_dirty = 1;
	} else if (c == 's' || c == 'S' || c == UI_KEY_DOWN) {
		for (int i = 0; i < 4; i++) { nx[i] = curx[i]; ny[i] = cury[i] + 1; }
		if (try_set(nx, ny)) g_dirty = 1;
		gravity_arm();
	} else if (c == 'w' || c == 'W' || c == UI_KEY_UP) {
		if (curtype != 1) {
			static const signed char PIVOT[7] = { 2, 0, 2, 3, 2, 2, 2 };
			int p = PIVOT[curtype];
			int px = curx[p], py = cury[p];
			int rx[4], ry[4];
			for (int i = 0; i < 4; i++) {
				rx[i] = px + (cury[i] - py);
				ry[i] = py - (curx[i] - px);
			}
			static const signed char KICK[11][2] = {
				{0,0},{-1,0},{1,0},{-2,0},{2,0},
				{0,-1},{0,-2},{0,1},{0,2},{-1,-1},{1,-1}
			};
			for (int k = 0; k < 11; k++) {
				for (int i = 0; i < 4; i++) {
					nx[i] = rx[i] + KICK[k][0];
					ny[i] = ry[i] + KICK[k][1];
				}
				if (try_set(nx, ny)) { g_dirty = 1; break; }
			}
		}
	} else if (c == ' ') {
		piece_hard_drop();
		if (piece_lock()) g_pending_over = 1;
	}
}

static int tetris_tick(void)
{
	if (g_pending_over)
		return GAME_OVER;

	int spd = gravity_spd();
	if (++t_gcd >= spd || t_hard) {
		t_gcd = 0; t_hard = 0;
		int nx[4], ny[4];
		for (int i = 0; i < 4; i++) { nx[i] = curx[i]; ny[i] = cury[i] + 1; }
		if (collide(nx, ny)) {
			if (piece_lock())
				return GAME_OVER;
		} else {
			for (int i = 0; i < 4; i++) cury[i]++;
			g_dirty = 1;
		}
	}

	if (g_dirty)
		tetris_push_state();
	return GAME_CONTINUE;
}

static void tetris_rect(int *row, int *col, int *width)
{
	int scr_rows, scr_cols;
	int inner, body_h, start, cluster_row;

	ui_app_query_screen(&scr_rows, &scr_cols);
	inner = scr_cols > 2 ? scr_cols - 2 : scr_cols;
	body_h = ui_screen_body_h(scr_rows);
	start = 1 + (inner - DISP_TETRIS_CLUSTER_W) / 2;
	if (start < 1)
		start = 1;
	cluster_row = UI_APP_CONTENT_ROW + (body_h - DISP_TETRIS_BOX_H) / 2;
	if (cluster_row < UI_APP_CONTENT_ROW)
		cluster_row = UI_APP_CONTENT_ROW;
	/* Top row inside the well (under the box border) — not vertical centre. */
	*row   = cluster_row + 1;
	*col   = start + DISP_TETRIS_INSTR_W + DISP_TETRIS_SIDE_GAP + 1;
	*width = DISP_TETRIS_DISP_W;
}

void app_tetris(void)
{
	static const Game g = { .frame_ms = TETRIS_LOOP_TICKS, .layout = 0,
	                        .reset = tetris_reset, .key = tetris_key,
	                        .tick = tetris_tick, .rect = tetris_rect };
	game_run(&g);
	display_tetris_end();
}

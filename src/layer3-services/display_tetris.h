#ifndef DISPLAY_TETRIS_H
#define DISPLAY_TETRIS_H

/*
 * Tetris view-model sent from the game task to the display server.
 * board[][] holds grounded blocks only (0 = empty, 1..7 = piece type).
 * Active piece position is curx/cury — not stored in board[][].
 *
 * Display scale: change DISP_TETRIS_CELL only (chars per block edge).
 * Well size, panel, and box glyphs are derived automatically.
 */
#define DISP_TETRIS_BW  10
#define DISP_TETRIS_BH  20
#define DISP_TETRIS_CELL  2   /* 2×2 corner blocks (┌┐ / └┘) */
#define DISP_TETRIS_DISP_W  (DISP_TETRIS_BW * DISP_TETRIS_CELL)
#define DISP_TETRIS_DISP_H  (DISP_TETRIS_BH * DISP_TETRIS_CELL)
#define DISP_TETRIS_PANEL_CELLS  4
#define DISP_TETRIS_PANEL_W      (DISP_TETRIS_PANEL_CELLS * DISP_TETRIS_CELL)
#define DISP_TETRIS_INSTR_W      16
#define DISP_TETRIS_SIDE_GAP     3
#define DISP_TETRIS_RIGHT_W      12
#define DISP_TETRIS_BOX_PAD      2   /* left + right (or top + bottom) border cols */
#define DISP_TETRIS_BOX_W        (DISP_TETRIS_DISP_W + DISP_TETRIS_BOX_PAD)
#define DISP_TETRIS_BOX_H        (DISP_TETRIS_DISP_H + DISP_TETRIS_BOX_PAD)
#define DISP_TETRIS_CLUSTER_W    (DISP_TETRIS_INSTR_W + DISP_TETRIS_SIDE_GAP \
                                    + DISP_TETRIS_BOX_W + DISP_TETRIS_SIDE_GAP \
                                    + DISP_TETRIS_RIGHT_W)

typedef struct {
	char board[DISP_TETRIS_BH][DISP_TETRIS_BW];
	int  curx[4];
	int  cury[4];
	signed char curtype;    /* 0..6 */
	signed char nexttype;   /* 0..6 */
	unsigned char active;   /* 1 = draw falling piece */
	unsigned      score;
	unsigned      lines;
	unsigned      level;
} DispTetrisState;

void display_tetris_begin(void);
void display_tetris_update(const DispTetrisState *st);
void display_tetris_end(void);

#endif

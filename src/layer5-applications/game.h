#ifndef GAME_H
#define GAME_H

/*
 * Layer 5 — game framework. The standard harness every game runs under, so all
 * games share one lifecycle and one set of controls:
 *
 *   - q / Q             quit to the terminal (any time)
 *   - the game's keys   passed to key() while playing
 *   - on game over      a centred "GAME OVER" banner + "[R]eplay  [Q]uit" prompt
 *   - R                 play again;   Q   exit
 *
 * Each game should follow textbook MVC (see mvc.h): Model holds state/rules,
 * View renders the Model, Controller implements the callbacks below and wires
 * input → Model → View. game_run() is the outer shell, not part of MVC.
 */
#define GAME_CONTINUE 0
#define GAME_OVER     1

typedef struct {
	int  frame_ms;                 /* per-frame delay (clock ticks) */
	void (*layout)(void);          /* optional: size playfield before reset runs */
	void (*reset)(void);           /* start a fresh game: clear, draw frame + initial state */
	void (*key)(int k);            /* handle one key (update state) */
	int  (*tick)(void);            /* advance + render one frame; GAME_CONTINUE / GAME_OVER */
	void (*rect)(int *row, int *col, int *width); /* play-area centre row, left col, width */
} Game;

void game_run(const Game *g);      /* the standard harness */

#endif

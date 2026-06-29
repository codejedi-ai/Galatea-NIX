#ifndef DISP_TETRIS_H
#define DISP_TETRIS_H

#include "display_tetris.h"

/* Display-server side (APU rendering). */
void disp_tetris_begin(void);
void disp_tetris_update(const DispTetrisState *st);
void disp_tetris_end(void);
int  disp_tetris_active(void);
void disp_tetris_render_shell(const char *status);

#endif

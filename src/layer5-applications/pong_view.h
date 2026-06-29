#ifndef PONG_VIEW_H
#define PONG_VIEW_H

#include "pong_model.h"
#include "layer4_ui.h"

typedef struct {
	UiCanvas cv;  /* cell backing store — paddles/ball written here each frame */
	int t0;       /* clock tick at game start */
} PongView;

void pong_view_reset(PongView *v, const PongModel *m);
void pong_view_render(PongView *v, const PongModel *m);
void pong_view_play_rect(const PongView *v, int *row, int *col, int *width);

#endif

#ifndef PONG_CONTROLLER_H
#define PONG_CONTROLLER_H

#include "pong_model.h"
#include "pong_view.h"

typedef struct {
	PongModel model;
	PongView  view;
} PongController;

void pong_controller_reset(PongController *c);
void pong_controller_key(PongController *c, int key);
int  pong_controller_tick(PongController *c);
void pong_controller_rect(PongController *c, int *row, int *col, int *width);

#endif

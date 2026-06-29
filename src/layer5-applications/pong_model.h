#ifndef PONG_MODEL_H
#define PONG_MODEL_H

#include "layer4_ui.h"

#define PONG_PAD         6
#define PONG_WIN         5
#define PONG_SPEED       1
#define PONG_PADDLE_HOLD 8
#define PONG_BALL_PERIOD 10  /* move ball every N game ticks (paddles every tick) */

typedef struct {
	int field_w, field_h;
	int p1_y, p2_y;
	int ball_x, ball_y;
	int ball_vx, ball_vy;
	int p1_score, p2_score;
	int p1_up_hold, p1_down_hold, p2_up_hold, p2_down_hold;
	int ball_cd;   /* ticks until next ball step */
} PongModel;

void pong_model_init(PongModel *m, int w, int h);
void pong_model_key(PongModel *m, int key);
void pong_model_tick(PongModel *m);
int  pong_model_is_over(const PongModel *m);

#endif

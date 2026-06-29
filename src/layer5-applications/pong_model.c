#include "pong_model.h"

static void pong_model_serve(PongModel *m, int toward_left)
{
	m->ball_x = m->field_w / 2;
	m->ball_y = m->field_h / 2;
	m->ball_vx = toward_left ? -1 : 1;
	m->ball_vy = 1;
}

static int pong_model_paddle_step(int *hold_up, int *hold_down)
{
	int mv = 0;
	if (*hold_up > 0)   { mv = -1; (*hold_up)--; }
	if (*hold_down > 0) { mv =  1; (*hold_down)--; }
	if (*hold_up > 0 && *hold_down > 0) mv = 0;
	return mv;
}

static void pong_model_ball_step(PongModel *m)
{
	int w = m->field_w, h = m->field_h;

	m->ball_x += m->ball_vx;
	m->ball_y += m->ball_vy;

	if (m->ball_y <= 0)      { m->ball_y = 0;      m->ball_vy =  1; }
	if (m->ball_y >= h - 1)  { m->ball_y = h - 1;  m->ball_vy = -1; }

	if (m->ball_x <= 1 && m->ball_vx < 0) {
		if (m->ball_y >= m->p1_y && m->ball_y < m->p1_y + PONG_PAD) {
			m->ball_x = 1;
			m->ball_vx = 1;
		} else if (m->ball_x <= 0) {
			m->p2_score++;
			pong_model_serve(m, 0);
		}
	}

	if (m->ball_x >= w - 2 && m->ball_vx > 0) {
		if (m->ball_y >= m->p2_y && m->ball_y < m->p2_y + PONG_PAD) {
			m->ball_x = w - 2;
			m->ball_vx = -1;
		} else if (m->ball_x >= w - 1) {
			m->p1_score++;
			pong_model_serve(m, 1);
		}
	}
}

void pong_model_init(PongModel *m, int w, int h)
{
	m->field_w = w;
	m->field_h = h;
	m->p1_y = h / 2 - PONG_PAD / 2;
	m->p2_y = h / 2 - PONG_PAD / 2;
	m->p1_score = 0;
	m->p2_score = 0;
	m->p1_up_hold = m->p1_down_hold = 0;
	m->p2_up_hold = m->p2_down_hold = 0;
	m->ball_cd = 0;
	pong_model_serve(m, 1);
}

void pong_model_key(PongModel *m, int key)
{
	if      (key == 'w' || key == 'W')  m->p1_up_hold = PONG_PADDLE_HOLD;
	else if (key == 's' || key == 'S')  m->p1_down_hold = PONG_PADDLE_HOLD;
	else if (key == UI_KEY_UP)           m->p2_up_hold = PONG_PADDLE_HOLD;
	else if (key == UI_KEY_DOWN)         m->p2_down_hold = PONG_PADDLE_HOLD;
}

void pong_model_tick(PongModel *m)
{
	int h = m->field_h;
	int m1 = pong_model_paddle_step(&m->p1_up_hold, &m->p1_down_hold);
	int m2 = pong_model_paddle_step(&m->p2_up_hold, &m->p2_down_hold);

	if (m1) {
		m->p1_y += m1 * PONG_SPEED;
		if (m->p1_y < 0)              m->p1_y = 0;
		if (m->p1_y > h - PONG_PAD)   m->p1_y = h - PONG_PAD;
	}
	if (m2) {
		m->p2_y += m2 * PONG_SPEED;
		if (m->p2_y < 0)              m->p2_y = 0;
		if (m->p2_y > h - PONG_PAD)   m->p2_y = h - PONG_PAD;
	}

	if (--m->ball_cd <= 0) {
		pong_model_ball_step(m);
		m->ball_cd = PONG_BALL_PERIOD;
	}
}

int pong_model_is_over(const PongModel *m)
{
	return m->p1_score >= PONG_WIN || m->p2_score >= PONG_WIN;
}

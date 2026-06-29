#include "pong_controller.h"
#include "layer4_ui.h"
#include "ui_app.h"
#include "../layer3-services/display_client.h"

void pong_controller_reset(PongController *c)
{
	int canvas_w, canvas_h;

	ui_app_canvas_fill_body(&canvas_w, &canvas_h);

	ui_canvas_init(&c->view.cv);
	c->view.cv.w = canvas_w;
	c->view.cv.h = canvas_h;

	pong_model_init(&c->model, canvas_w, canvas_h);
	pong_view_reset(&c->view, &c->model);
}

void pong_controller_key(PongController *c, int key)
{
	pong_model_key(&c->model, key);
}

int pong_controller_tick(PongController *c)
{
	pong_model_tick(&c->model);
	pong_view_render(&c->view, &c->model);
	return pong_model_is_over(&c->model) ? 1 : 0;
}

void pong_controller_rect(PongController *c, int *row, int *col, int *width)
{
	pong_view_play_rect(&c->view, row, col, width);
}

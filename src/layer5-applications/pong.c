#include "apps.h"
#include "game.h"
#include "pong_controller.h"

/*
 * Pong — Layer 5 app entry. MVC split:
 *   pong_model.c      Model (state + rules)
 *   pong_view.c       View (canvas / diff render)
 *   pong_controller.c Controller (input + tick glue)
 */

static PongController g_pong;

static void pong_reset(void)
{
	pong_controller_reset(&g_pong);
}

static void pong_key(int k)
{
	pong_controller_key(&g_pong, k);
}

static int pong_tick(void)
{
	return pong_controller_tick(&g_pong) ? GAME_OVER : GAME_CONTINUE;
}

static void pong_rect(int *row, int *col, int *width)
{
	pong_controller_rect(&g_pong, row, col, width);
}

void app_pong(void)
{
	static const Game g = { .frame_ms = 5, .reset = pong_reset, .key = pong_key,
	                        .tick = pong_tick, .rect = pong_rect };
	game_run(&g);
}

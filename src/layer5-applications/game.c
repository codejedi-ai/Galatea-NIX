#include "game.h"
#include "layer4_ui.h"
#include "display_client.h"
#include "clock_client.h"
#include "clockserver.h"
#include "../layer1-processes/syscall.h"
#include "../layer1-processes/rpi.h"

static void game_overlay_c(int row, int col, int width, const char *text,
                             const char *attrs)
{
	int len = 0;
	while (text[len]) len++;
	int x = col + (width - len) / 2;
	if (x < col) x = col;
	display_put_str(row, x, text, attrs);
	display_flush();
}

void game_run(const Game *g)
{
	int clock = ClockServerTid();
	ui_begin();

	for (;;) {                              /* replay loop */
		if (g->layout)
			g->layout();
		g->reset();

		int paused = 0;
		while (1) {                         /* play loop */
			int k;
			while ((k = ui_trygetch()) >= 0) {
				if (k == 'q' || k == 'Q') { ui_end(); return; }
				if (k == 'p' || k == 'P') {
					int row = 0, col = 0, width = 0;
					g->rect(&row, &col, &width);
					paused = !paused;
					if (paused)
						game_overlay_c(row, col, width, " PAUSED ",
						               "\033[1;37;44m");
					else
						game_overlay_c(row, col, width, "        ", 0);
					continue;
				}
				if (!paused) g->key(k);
			}
			if (!paused && g->tick() == GAME_OVER) break;
			Delay(clock, g->frame_ms);
		}

		{
			int row = 0, col = 0, width = 0;
			g->rect(&row, &col, &width);
			game_overlay_c(row, col, width, " GAME OVER ",
			               "\033[1;37;41m");
		}
		{
			int row = 0, col = 0, width = 0;
			g->rect(&row, &col, &width);
			if (ui_replay_prompt(row + 2, col, width) == 'q') { ui_end(); return; }
		}
	}
}

#ifndef UI_SCREEN_H
#define UI_SCREEN_H

/*
 * Canonical OS screen dimensions — single include for apps, shell, display server.
 * Default size in config.h: UI_DESIGN_SCREEN_COLS × UI_DESIGN_SCREEN_ROWS (176×50).
 *   body = screen minus meter + DIV chrome
 */

#include "../layer1-processes/config.h"

#define UI_SCREEN_COLS          UI_DESIGN_SCREEN_COLS
#define UI_SCREEN_ROWS          UI_DESIGN_SCREEN_ROWS
#define UI_SCREEN_BODY_W        UI_DESIGN_BODY_W
#define UI_SCREEN_BODY_H        UI_DESIGN_BODY_H

#define ui_screen_inner_w(cols) ((cols) > 2 ? (cols) - 2 : (cols))
#define ui_screen_body_h(rows)  ((rows) - UI_DESIGN_METER_ROWS - UI_DESIGN_DIV_CHROME)

#endif

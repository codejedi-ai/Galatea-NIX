#ifndef UI_APP_H
#define UI_APP_H

#include "ui_elem.h"
#include "ui_screen.h"

/*
 * Base app chrome (all GUI apps including Terminal):
 *   row 0      — CPU0–CPU3 meters (UI_SCREEN auto-prepends UI_METERS)
 *   rows 1..N  — edge-to-edge UI_DIV frame (.h fills the rest of the display)
 *
 * Build content nodes, wrap with ui_app_shell_init(), render with
 * ui_app_shell_render(). Call ui_app_enter() / ui_app_leave() around the app.
 */

#define UI_APP_FRAME_H(rows)     ((rows) - UI_SCREEN_METER_ROWS)
#define UI_APP_BODY_H(rows)      ui_screen_body_h(rows)
#define UI_APP_INNER_W(cols)     ui_screen_inner_w(cols)
/* First screen row inside the DIV side borders (canvas / text body). */
#define UI_APP_CONTENT_ROW       (UI_SCREEN_METER_ROWS + 3)

typedef struct {
	UiElem screen;
	UiElem frame;
} UiAppShell;

void ui_app_enter(void);
void ui_app_leave(void);

void ui_app_shell_init(UiAppShell *sh, const char *title, const char *status,
                       int screen_rows, UiElem *content, int n_content);

void ui_app_shell_update(UiAppShell *sh, const char *status, int screen_rows);

void ui_app_shell_render(const UiAppShell *sh, int full);

int ui_app_content_center_row(int content_h);

/* Query display-server screen size and derived app-body dimensions. */
void ui_app_query_screen(int *rows, int *cols);
void ui_app_query_body(int *body_w, int *body_h);

/* Fill the app body with a canvas (Snake, Pong, launcher grids). */
void ui_app_canvas_fill_body(int *out_w, int *out_h);

/* Largest w×h inside avail_w×avail_h preserving aspect_w:aspect_h (terminal cells). */
void ui_canvas_fit_aspect(int avail_w, int avail_h,
                          int aspect_w, int aspect_h,
                          int min_w, int min_h,
                          int *out_w, int *out_h);

#endif

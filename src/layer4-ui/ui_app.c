#include "ui_app.h"
#include "display_client.h"
#include "config.h"
#include "ui_screen.h"

/* 16:10 base units; default screen size is in config.h UI_DESIGN_SCREEN_*. */
_Static_assert(UI_DESIGN_SCREEN_COLS >= UI_DESIGN_MIN_COLS,
               "screen must be wide enough for train ASCII art");

void ui_app_enter(void)
{
	display_clear();
	display_cursor_hide();
}

void ui_app_leave(void)
{
	display_clear();
	display_cursor_hide();
}

void ui_app_shell_init(UiAppShell *sh, const char *title, const char *status,
                       int screen_rows, UiElem *content, int n_content)
{
	if (!sh)
		return;
	sh->frame = (UiElem){
		.type       = UI_DIV,
		.title      = title,
		.status     = status,
		.h          = UI_APP_FRAME_H(screen_rows),
		.children   = content,
		.n_children = n_content,
	};
	sh->screen = (UiElem){
		.type       = UI_SCREEN,
		.children   = &sh->frame,
		.n_children = 1,
	};
}

void ui_app_shell_update(UiAppShell *sh, const char *status, int screen_rows)
{
	if (!sh)
		return;
	sh->frame.status = status;
	sh->frame.h      = UI_APP_FRAME_H(screen_rows);
}

void ui_app_shell_render(const UiAppShell *sh, int full)
{
	if (!sh)
		return;
	if (full)
		display_render_tree_full((UiElem *)&sh->screen);
	else
		display_render_tree((UiElem *)&sh->screen);
}

int ui_app_content_center_row(int content_h)
{
	return UI_APP_CONTENT_ROW + content_h / 2;
}

void ui_app_query_screen(int *rows, int *cols)
{
	display_get_size(rows, cols);
}

void ui_app_query_body(int *body_w, int *body_h)
{
	int rows = 0, cols = 0;

	ui_app_query_screen(&rows, &cols);
	if (body_h)
		*body_h = UI_APP_BODY_H(rows);
	if (body_w)
		*body_w = UI_APP_INNER_W(cols);
}

void ui_app_canvas_fill_body(int *out_w, int *out_h)
{
	int w = 0, h = 0;

	ui_app_query_body(&w, &h);
	if (h < UI_CANVAS_HMIN)
		h = UI_CANVAS_HMIN;
	if (h > UI_CANVAS_HMAX)
		h = UI_CANVAS_HMAX;
	if (w < 4)
		w = 4;
	if (w > UI_CANVAS_WMAX)
		w = UI_CANVAS_WMAX;
	if (out_w)
		*out_w = w;
	if (out_h)
		*out_h = h;
}

void ui_canvas_fit_aspect(int avail_w, int avail_h,
                          int aspect_w, int aspect_h,
                          int min_w, int min_h,
                          int *out_w, int *out_h)
{
	int w, h;

	if (aspect_w < 1)
		aspect_w = 1;
	if (aspect_h < 1)
		aspect_h = 1;
	if (avail_w < 1)
		avail_w = 1;
	if (avail_h < 1)
		avail_h = 1;
	if (min_w < 1)
		min_w = 1;
	if (min_h < 1)
		min_h = 1;

	/* Scale to fit avail, preserving aspect (contain). */
	w = avail_w;
	h = (w * aspect_h) / aspect_w;
	if (h > avail_h) {
		h = avail_h;
		w = (h * aspect_w) / aspect_h;
	}

	if (w < min_w) {
		w = min_w;
		h = (w * aspect_h) / aspect_w;
		if (h > avail_h) {
			h = avail_h;
			w = (h * aspect_w) / aspect_h;
		}
	}
	if (h < min_h) {
		h = min_h;
		w = (h * aspect_w) / aspect_h;
		if (w > avail_w) {
			w = avail_w;
			h = (w * aspect_h) / aspect_w;
		}
	}

	*out_w = w;
	*out_h = h;
}

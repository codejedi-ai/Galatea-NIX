#ifndef DISPLAY_CLIENT_H
#define DISPLAY_CLIENT_H

struct UiElem;  /* full type in ui_elem.h — include it if you need UiElem fields */

/* ── Legacy ANSI-passthrough path (shell / snake / tetris) ──────────────────
 * All terminal output goes through the display server (screenbuf + g_term).
 * Prefer display_write_str / display_readline_* over raw uart_printf(CONSOLE). */
void disp_write(const char *data, int len);
void disp_render(void);
void display_client_register_route(void);

/* ── Declarative MVC path (display server renders UiElem tree) ────────────── */
/* UI_SCREEN auto-prepends UI_METERS (CPU0–CPU3, UI_SEG_* quarters) unless a
 * UI_METERS child is already present. Terminal/shell output does not use this. */

/* Send a UiElem tree to the display server for rendering.
 * The tree may live on the caller's stack — this is a synchronous Send/Reply
 * so all pointers are valid until this function returns.
 * display_render_tree()      — incremental: emit only cells that changed.
 * display_render_tree_full() — force a full repaint (use on the first frame
 *                              of a new screen, or after a resize). */
void display_render_tree(struct UiElem *root);
void display_render_tree_full(struct UiElem *root);

/* Blank the whole screen and sync the server's model to it. Use when entering
 * the terminal or a new app so no stale cells survive the transition. */
void display_clear(void);

/* Scroll the terminal's scrollback viewport by `delta` lines (+ older / - newer).
 * Fresh terminal output snaps the viewport back to the live bottom. */
void display_scroll(int delta);

/* Ask the display server for the current terminal grid (rows × cols).
 * Never hangs: falls back to UI_DESIGN_SCREEN_* if the server is not running. */
void display_get_size(int *rows, int *cols);

/* Query the physical terminal (ESC[6n) via the display server; syncs its buffer
 * dimensions without clearing scrollback content. Falls back to display_get_size. */
void display_query_terminal(int *rows, int *cols);

/* Fix the resolution (disables autofit until display_screen_auto). */
void display_set_size(int cols, int rows);

/* Re-enable autofit and query the physical terminal size. */
void display_screen_auto(void);

/* Restore screen width saved under /disk/screen.cols (16:10 rows from cols). */
void display_restore_persisted_screen(void);

/* 1 if a manual resolution is active in the display server. */
int display_screen_is_manual(void);

/* Write a NUL-terminated string through the display server (legacy term path). */
void display_write_str(const char *s);

#include "display_tetris.h"

/* Patch one cell in the server's next[] buffer (row/col 0-based). attrs may be NULL. */
void display_put_cell(int row, int col, char ch, const char *attrs);

/* Write a string into next[] starting at (row, col). attrs may be NULL. */
void display_put_str(int row, int col, const char *s, const char *attrs);

/* Diff next[] against cell[] and emit only changed cells to the terminal. */
void display_flush(void);

/* Block until the user presses a key; the display server reads the console. */
int display_wait_key(void);

/* Shell readline: show the block cursor and remember the column after the prompt
 * so backspace cannot erase into the prompt text. */
void display_cursor_show(void);
void display_cursor_hide(void);
void display_readline_start(void);
/* Limit shell scrolling/output to body rows [top..bottom] (0-based, inclusive). */
void display_shell_region(int top_row, int bottom_row);
void display_goto_cursor(int row, int col);   /* 0-based; updates server g_term   */
void display_get_cursor(int *row, int *col, int *min_col);
void display_readline_echo(char c);           /* echo byte; server tracks cursor  */
void display_readline_end(void);              /* after newline; clears readline   */
void display_erase_char(void);

#endif

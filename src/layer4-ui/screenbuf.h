#ifndef SCREENBUF_H
#define SCREENBUF_H

#define SBUF_ROWS 60
#define SBUF_COLS 200
#define SBUF_ATTRS 16

/*
 * Three-buffer character render (display server only):
 *
 *   cell[]  — current: what the terminal is showing now (last diff applied)
 *   next[]  — new:     desired frame built by tree/canvas/overlay writers
 *   render  — diff:    screenbuf_render() emits only cell[] != next[], then
 *                       copies next[] → cell[]
 *
 * Typical frame: screenbuf_clear_next() or screenbuf_copy_rect() seeds next[],
 * writers patch next[], APU1–3 enqueue cell[]≠next[] into a UART update queue,
 * CPU0 drains the queue (screenbuf_render) — no full-screen dirty scan on CPU0.
 */

typedef struct {
	unsigned char g[3];
	unsigned char a[SBUF_ATTRS];
} ScreenCell;

typedef struct {
	int rows, cols;
	ScreenCell cell[SBUF_ROWS][SBUF_COLS];  /* current — on-screen state       */
	ScreenCell next[SBUF_ROWS][SBUF_COLS];  /* new     — frame under construction */
} ScreenBuf;

void screenbuf_init(ScreenBuf *sb, int rows, int cols);
void screenbuf_set_size(ScreenBuf *sb, int rows, int cols); /* resize without clearing */
void screenbuf_clear_buf(ScreenBuf *sb);   /* clear next[], invalidate cell[] -> full repaint */
void screenbuf_clear_next(ScreenBuf *sb);  /* clear next[] ONLY -> per-frame incremental diff  */
void screenbuf_sync_blank(ScreenBuf *sb);  /* blank BOTH next[] and cell[] (screen known blank) */
void screenbuf_scroll_up(ScreenBuf *sb, int n);  /* scroll next[] up n rows, blank the bottom   */
void screenbuf_scroll_span_up(ScreenBuf *sb, int r0, int r1); /* scroll rows r0..r1 up one */
void screenbuf_scroll_span_cols_up(ScreenBuf *sb, int r0, int r1, int c0, int c1);
void screenbuf_scroll_cell_up(ScreenBuf *sb, int n); /* shift the rendered cell[] up in lockstep */
void screenbuf_scroll_span_cell_up(ScreenBuf *sb, int r0, int r1);
void screenbuf_scroll_span_cols_cell_up(ScreenBuf *sb, int r0, int r1, int c0, int c1);
void screenbuf_set(ScreenBuf *sb, int row, int col, const unsigned char *glyph, const unsigned char *attrs);
void screenbuf_clear_eol(ScreenBuf *sb, int row, int col);
/* Copy current → new for a rectangle (canvas diff: unchanged cells skip UART). */
void screenbuf_copy_rect(ScreenBuf *sb, int row, int col, int w, int h);

typedef void (*ScreenEmitFn)(const char *s);
/* APU1–3 build a per-row-band diff queue; CPU0 drains it to UART (no full scan). */
void screenbuf_render(ScreenBuf *sb, ScreenEmitFn emit);

#endif

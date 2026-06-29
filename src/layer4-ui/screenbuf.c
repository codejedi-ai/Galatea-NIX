#include "screenbuf.h"

static void sb_memcpy(void *dst, const void *src, int n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	for (int i = 0; i < n; i++) d[i] = s[i];
}

static void sb_cell_space(ScreenCell *c)
{
	c->g[0] = ' ';
	c->g[1] = 0;
	c->g[2] = 0;
	c->a[0] = 0;
}

void screenbuf_init(ScreenBuf *sb, int rows, int cols)
{
	if (rows > SBUF_ROWS) rows = SBUF_ROWS;
	if (cols > SBUF_COLS) cols = SBUF_COLS;
	sb->rows = rows;
	sb->cols = cols;
	screenbuf_clear_buf(sb);
}

void screenbuf_set_size(ScreenBuf *sb, int rows, int cols)
{
	if (rows > SBUF_ROWS) rows = SBUF_ROWS;
	if (cols > SBUF_COLS) cols = SBUF_COLS;
	if (rows < 1) rows = 1;
	if (cols < 1) cols = 1;
	sb->rows = rows;
	sb->cols = cols;
}

void screenbuf_clear_buf(ScreenBuf *sb)
{
	for (int r = 0; r < sb->rows; r++) {
		for (int c = 0; c < sb->cols; c++) {
			sb_cell_space(&sb->next[r][c]);
			sb->cell[r][c].g[0] = 0;  /* force diff on next render */
			sb->cell[r][c].a[0] = 0;
		}
	}
}

/*
 * Clear ONLY the desired buffer (next[]), leaving the last-rendered state
 * (cell[]) intact. This is the per-frame reset for tree rendering: after a
 * fresh tree is drawn into next[], screenbuf_render() diffs against cell[]
 * and emits only the cells that actually changed since the previous frame.
 */
void screenbuf_clear_next(ScreenBuf *sb)
{
	for (int r = 0; r < sb->rows; r++)
		for (int c = 0; c < sb->cols; c++)
			sb_cell_space(&sb->next[r][c]);
}

/*
 * Mark the whole screen as known-blank: both buffers become spaces. Pairs
 * with a physical "\033[2J" emit so cell[] (our model of the terminal) and
 * the real terminal agree. The next incremental render then only paints the
 * non-blank cells of the new screen.
 */
void screenbuf_sync_blank(ScreenBuf *sb)
{
	for (int r = 0; r < sb->rows; r++)
		for (int c = 0; c < sb->cols; c++) {
			sb_cell_space(&sb->next[r][c]);
			sb_cell_space(&sb->cell[r][c]);
		}
}

/* Scroll the desired buffer up by n rows; blank the freed bottom rows. The
 * diff in screenbuf_render() turns this into the minimal set of cell writes. */
void screenbuf_scroll_up(ScreenBuf *sb, int n)
{
	if (n <= 0) return;
	if (n > sb->rows) n = sb->rows;
	for (int r = 0; r < sb->rows - n; r++)
		for (int c = 0; c < sb->cols; c++)
			sb_memcpy(&sb->next[r][c], &sb->next[r + n][c], sizeof(ScreenCell));
	for (int r = sb->rows - n; r < sb->rows; r++)
		for (int c = 0; c < sb->cols; c++)
			sb_cell_space(&sb->next[r][c]);
}

void screenbuf_scroll_span_up(ScreenBuf *sb, int r0, int r1)
{
	if (r0 < 0 || r1 >= sb->rows || r1 <= r0) return;
	for (int r = r0; r < r1; r++)
		for (int c = 0; c < sb->cols; c++)
			sb_memcpy(&sb->next[r][c], &sb->next[r + 1][c], sizeof(ScreenCell));
	for (int c = 0; c < sb->cols; c++)
		sb_cell_space(&sb->next[r1][c]);
}

void screenbuf_scroll_span_cols_up(ScreenBuf *sb, int r0, int r1, int c0, int c1)
{
	if (r0 < 0 || r1 >= sb->rows || r1 <= r0) return;
	if (c0 < 0) c0 = 0;
	if (c1 >= sb->cols) c1 = sb->cols - 1;
	if (c1 < c0) return;
	for (int r = r0; r < r1; r++)
		for (int c = c0; c <= c1; c++)
			sb_memcpy(&sb->next[r][c], &sb->next[r + 1][c], sizeof(ScreenCell));
	for (int c = c0; c <= c1; c++)
		sb_cell_space(&sb->next[r1][c]);
}

/*
 * Shift the last-rendered buffer (cell[]) up by n rows too, blanking the bottom.
 * Pairs with a real terminal scroll (CSI / LF): once cell[] matches the scrolled
 * physical screen, screenbuf_render() only repaints the genuinely new bottom
 * rows instead of re-emitting every shifted cell — real-terminal scrolling.
 */
void screenbuf_scroll_cell_up(ScreenBuf *sb, int n)
{
	if (n <= 0) return;
	if (n > sb->rows) n = sb->rows;
	for (int r = 0; r < sb->rows - n; r++)
		for (int c = 0; c < sb->cols; c++)
			sb_memcpy(&sb->cell[r][c], &sb->cell[r + n][c], sizeof(ScreenCell));
	for (int r = sb->rows - n; r < sb->rows; r++)
		for (int c = 0; c < sb->cols; c++)
			sb_cell_space(&sb->cell[r][c]);
}

void screenbuf_scroll_span_cell_up(ScreenBuf *sb, int r0, int r1)
{
	if (r0 < 0 || r1 >= sb->rows || r1 <= r0) return;
	for (int r = r0; r < r1; r++)
		for (int c = 0; c < sb->cols; c++)
			sb_memcpy(&sb->cell[r][c], &sb->cell[r + 1][c], sizeof(ScreenCell));
	for (int c = 0; c < sb->cols; c++)
		sb_cell_space(&sb->cell[r1][c]);
}

void screenbuf_scroll_span_cols_cell_up(ScreenBuf *sb, int r0, int r1, int c0, int c1)
{
	if (r0 < 0 || r1 >= sb->rows || r1 <= r0) return;
	if (c0 < 0) c0 = 0;
	if (c1 >= sb->cols) c1 = sb->cols - 1;
	if (c1 < c0) return;
	for (int r = r0; r < r1; r++)
		for (int c = c0; c <= c1; c++)
			sb_memcpy(&sb->cell[r][c], &sb->cell[r + 1][c], sizeof(ScreenCell));
	for (int c = c0; c <= c1; c++)
		sb_cell_space(&sb->cell[r1][c]);
}

void screenbuf_set(ScreenBuf *sb, int row, int col, const unsigned char *glyph, const unsigned char *attrs)
{
	if (row < 0 || row >= sb->rows || col < 0 || col >= sb->cols) return;
	int i;
	for (i = 0; i < 3 && glyph && glyph[i]; i++) sb->next[row][col].g[i] = glyph[i];
	while (i < 3) sb->next[row][col].g[i++] = 0;
	for (i = 0; i < SBUF_ATTRS && attrs && attrs[i]; i++) sb->next[row][col].a[i] = attrs[i];
	while (i < SBUF_ATTRS) sb->next[row][col].a[i++] = 0;
}

void screenbuf_clear_eol(ScreenBuf *sb, int row, int col)
{
	if (row < 0 || row >= sb->rows) return;
	if (col < 0) col = 0;
	for (int c = col; c < sb->cols; c++)
		screenbuf_set(sb, row, c, (const unsigned char *)" ", (const unsigned char *)"");
}

void screenbuf_copy_rect(ScreenBuf *sb, int row, int col, int w, int h)
{
	if (w <= 0 || h <= 0)
		return;
	if (row < 0) { h += row; row = 0; }
	if (col < 0) { w += col; col = 0; }
	if (row + h > sb->rows) h = sb->rows - row;
	if (col + w > sb->cols)  w = sb->cols - col;
	if (h <= 0 || w <= 0)
		return;
	for (int r = 0; r < h; r++)
		for (int c = 0; c < w; c++)
			sb->next[row + r][col + c] = sb->cell[row + r][col + c];
}

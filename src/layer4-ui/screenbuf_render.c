#include "screenbuf.h"
#include "../layer3-services/apu_client.h"

#define SBUF_DIFF_BANDS  3   /* APU cores 1, 2, 3 — one horizontal band each */
#define SBUF_DIFF_QCAP     8192

typedef struct {
	int row, col;
	ScreenCell cell;
} ScreenDiffEntry;

static ScreenDiffEntry g_diff_q[SBUF_DIFF_BANDS][SBUF_DIFF_QCAP];
static int g_diff_n[SBUF_DIFF_BANDS];

typedef struct {
	ScreenBuf *sb;
	int band;
	int row_start, row_end;
} DiffBandArgs;

static DiffBandArgs g_diff_args[SBUF_DIFF_BANDS];

static void sb_memcpy_local(void *dst, const void *src, int n)
{
	unsigned char *d = dst;
	const unsigned char *s = src;
	for (int i = 0; i < n; i++)
		d[i] = s[i];
}

static int sb_memcmp_local(const void *a, const void *b, int n)
{
	const unsigned char *pa = a, *pb = b;
	for (int i = 0; i < n; i++)
		if (pa[i] != pb[i])
			return pa[i] - pb[i];
	return 0;
}

/* APU worker: compare current[] vs new[] for one row band; enqueue dirty cells. */
static void diff_band_fn(void *arg)
{
	DiffBandArgs *b = arg;
	ScreenBuf *sb = b->sb;
	int n = 0;

	for (int r = b->row_start; r < b->row_end; r++) {
		for (int c = 0; c < sb->cols; c++) {
			if (!sb_memcmp_local(&sb->cell[r][c], &sb->next[r][c],
			                     sizeof(ScreenCell))) {
				continue;
			}
			if (n >= SBUF_DIFF_QCAP)
				continue;
			g_diff_q[b->band][n].row  = r;
			g_diff_q[b->band][n].col  = c;
			g_diff_q[b->band][n].cell = sb->next[r][c];
			n++;
		}
	}
	g_diff_n[b->band] = n;
}

static void emit_csi(ScreenEmitFn emit, int row, int col)
{
	char seq[48];
	int p = 0;
	int n;
	int d[4];
	int nd;

	seq[p++] = '\033';
	seq[p++] = '[';
	n = row + 1;
	nd = 0;
	if (n == 0)
		d[nd++] = '0';
	while (n) {
		d[nd++] = (char)('0' + n % 10);
		n /= 10;
	}
	while (nd)
		seq[p++] = d[--nd];
	seq[p++] = ';';
	n = col + 1;
	nd = 0;
	if (n == 0)
		d[nd++] = '0';
	while (n) {
		d[nd++] = (char)('0' + n % 10);
		n /= 10;
	}
	while (nd)
		seq[p++] = d[--nd];
	seq[p++] = 'H';
	seq[p] = 0;
	emit(seq);
}

static void emit_queued_cell(ScreenBuf *sb, ScreenEmitFn emit,
                             const ScreenDiffEntry *e)
{
	emit_csi(emit, e->row, e->col);
	if (e->cell.a[0])
		emit((const char *)e->cell.a);
	{
		char gz[4];
		int gi = 0;
		while (gi < 3 && e->cell.g[gi]) {
			gz[gi] = (char)e->cell.g[gi];
			gi++;
		}
		gz[gi] = 0;
		emit(gz);
	}
	emit("\033[0m");
	sb_memcpy_local(&sb->cell[e->row][e->col], &e->cell, sizeof(ScreenCell));
}

/* CPU0: drain the UART update queue built by APU1–3 (no screen scan here). */
static void flush_uart_queue(ScreenBuf *sb, ScreenEmitFn emit)
{
	emit("\033[?2026h");
	for (int band = 0; band < SBUF_DIFF_BANDS; band++) {
		for (int i = 0; i < g_diff_n[band]; i++)
			emit_queued_cell(sb, emit, &g_diff_q[band][i]);
	}
	emit("\033[?2026l");
}

void screenbuf_render(ScreenBuf *sb, ScreenEmitFn emit)
{
	int band_h, rem, row;

	if (!sb || sb->rows <= 0 || sb->cols <= 0 || !emit)
		return;

	for (int band = 0; band < SBUF_DIFF_BANDS; band++)
		g_diff_n[band] = 0;

	band_h = sb->rows / SBUF_DIFF_BANDS;
	rem    = sb->rows % SBUF_DIFF_BANDS;
	row    = 0;

	ApuJob jobs[SBUF_DIFF_BANDS];
	for (int band = 0; band < SBUF_DIFF_BANDS; band++) {
		int h = band_h + (band < rem ? 1 : 0);
		g_diff_args[band].sb        = sb;
		g_diff_args[band].band      = band;
		g_diff_args[band].row_start = row;
		g_diff_args[band].row_end   = row + h;
		row += h;
		jobs[band].fn  = diff_band_fn;
		jobs[band].arg = &g_diff_args[band];
	}
	APUBatch(jobs, SBUF_DIFF_BANDS);
	flush_uart_queue(sb, emit);
}

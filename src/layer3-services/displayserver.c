#include "displayserver.h"
#include "display_messages.h"
#include "display_client.h"
#include "disp_tetris.h"
#include "nameserver.h"
#include "../layer2-messaging/messaging.h"
#include "../layer2-messaging/console_route.h"
#include "../layer4-ui/screenbuf.h"
#include "../layer4-ui/ui_elem.h"
#include "../layer4-ui/ui_app.h"
#include "../layer1-processes/syscall.h"
#include "../layer1-processes/config.h"
#include "../layer1-processes/rpi.h"
#include "ui_screen.h"
#include "apu_client.h"                  /* APU dispatch for canvas rendering */
#include "../layer1-processes/pmm.h"
#include "../layer1-processes/malloc/malloc.h"
#include "idle.h"                         /* g_idle_us / time_us_since_boot — meter row */
#include "clockserver.h"
#include "clock_client.h"
#include "diskfs.h"

#define DS_SCREEN_COLS_PATH  "/screen.cols"

static void ds_screen_cols_write(int cols)
{
	char buf[12];
	int p = 0;
	unsigned v = (unsigned)cols;

	if (cols < 16 || cols > 200)
		return;
	if (v >= 100)
		buf[p++] = (char)('0' + v / 100);
	if (v >= 10)
		buf[p++] = (char)('0' + (v / 10) % 10);
	buf[p++] = (char)('0' + (v % 10));
	buf[p++] = '\n';
	DiskWrite(DS_SCREEN_COLS_PATH, buf, p);
}

/* ================================================================ globals == */

static int display_server_tid = -1;
ScreenBuf g_sb;

/* Canonical terminal grid — initialised at boot, updated by set/auto/query. */
int g_display_rows;
int g_display_cols;
int g_display_manual;

/* ANSI-parsing cursor state (legacy WRITE path). The escape/UTF-8 parser fields
 * are PERSISTENT across term_feed() calls so a CSI sequence or multi-byte glyph
 * split across two WRITE chunks (the client chunks at 240 bytes) resumes cleanly
 * instead of being corrupted. */
enum { ST_NORMAL, ST_ESC, ST_CSI };
typedef struct {
	int row, col;
	unsigned char attrs[SBUF_ATTRS];
	int  st;                /* ST_NORMAL / ST_ESC / ST_CSI                    */
	int  params[6];
	int  param_n;
	int  cur_param;
	unsigned char upend[3]; /* in-progress UTF-8 bytes                         */
	int  upend_n;           /* bytes collected so far                          */
	int  upend_need;        /* total bytes the lead byte announced (2/3/4)     */
	int  sync;              /* DEC 2026 synchronized-output mode active         */
} TermState;
static TermState g_term;

static void term_reset_parser(void)
{
	g_term.st = ST_NORMAL;
	g_term.param_n = 0;
	g_term.cur_param = 0;
	g_term.upend_n = 0;
	g_term.upend_need = 0;
	g_term.sync = 0;
}

/* ── terminal scrollback ────────────────────────────────────────────────────
 * The terminal's screen model is a large line buffer: lines that scroll off the
 * top are kept in a ring so the user can scroll back through history with the
 * up/down arrows. g_tview is the viewport offset from the live bottom (0 = live).
 * g_tsave snapshots the live screen while scrolled so it can be restored. */
#define THIST_LINES 256
static ScreenCell g_thist[THIST_LINES][SBUF_COLS];
static int g_thist_head;    /* ring index of the oldest history line          */
static int g_thist_count;   /* number of history lines (<= THIST_LINES)        */
static ScreenCell g_tsave[SBUF_ROWS][SBUF_COLS];
static int g_tview;         /* lines scrolled back from live (0 = live)        */
static int g_cursor_visible; /* DECTCEM: 1 = show block cursor (shell readline) */
static int g_input_min_col;  /* backspace cannot move left of this column       */
static int g_shell_r0;       /* shell body scroll region top (0-based), -1=off  */
static int g_shell_r1;       /* shell body scroll region bottom (inclusive)       */
static int g_readline_row;   /* cursor row while editing (0-based)                 */
static int g_readline_col;   /* cursor col while editing                           */
static int g_readline_start_row; /* first row of this input (prompt line)        */

/* ================================================================ exports == */

int DisplayServerTid(void)          { return display_server_tid; }
int display_server_in_handler(void) { return 0; }

/* ================================================================== emit == */

static void disp_emit_str(const char *s)
{
	if (s) uart_puts(CONSOLE, (char *)s);
}

static void disp_cursor_apply(void)
{
	disp_emit_str(g_cursor_visible ? "\033[?25h" : "\033[?25l");
}

static void term_emit_cursor_pos(void)
{
	char seq[32];
	int p = 0;
	int r = g_term.row + 1;
	int c = g_term.col + 1;

	seq[p++] = '\033';
	seq[p++] = '[';
	if (r >= 100) seq[p++] = (char)('0' + r / 100);
	if (r >= 10)  seq[p++] = (char)('0' + (r / 10) % 10);
	seq[p++] = (char)('0' + r % 10);
	seq[p++] = ';';
	if (c >= 100) seq[p++] = (char)('0' + c / 100);
	if (c >= 10)  seq[p++] = (char)('0' + (c / 10) % 10);
	seq[p++] = (char)('0' + c % 10);
	seq[p++] = 'H';
	seq[p] = 0;
	disp_emit_str(seq);
}

static void disp_render_now(void)
{
	console_route_bypass(1);
	if (!g_cursor_visible)
		disp_emit_str("\033[?25l");
	/* APU1–3 diff cell[] vs next[] into a queue; CPU0 drains UART here. */
	screenbuf_render(&g_sb, disp_emit_str);
	if (g_cursor_visible)
		term_emit_cursor_pos();
	disp_cursor_apply();
	console_route_bypass(0);
}

/* ======================================================= screenbuf helpers == */

static void sb_char(int row, int col, char ch, const char *attrs)
{
	unsigned char g[3];
	unsigned char a[SBUF_ATTRS];
	int i = 0;
	g[0] = (unsigned char)ch; g[1] = 0; g[2] = 0;
	if (attrs) while (attrs[i] && i < SBUF_ATTRS - 1)
		{ a[i] = (unsigned char)attrs[i]; i++; }
	a[i] = 0;
	screenbuf_set(&g_sb, row, col, g, a);
}

/* Write one UTF-8 glyph (up to 3 bytes) as a single screenbuf cell. */
static void sb_utf8(int row, int col, const char *u, const char *attrs)
{
	unsigned char g[3];
	unsigned char a[SBUF_ATTRS];
	int i = 0;
	g[0] = (unsigned char)u[0];
	g[1] = u[1] ? (unsigned char)u[1] : 0;
	g[2] = u[2] ? (unsigned char)u[2] : 0;
	if (attrs) while (attrs[i] && i < SBUF_ATTRS - 1)
		{ a[i] = (unsigned char)attrs[i]; i++; }
	a[i] = 0;
	screenbuf_set(&g_sb, row, col, g, a);
}

/* ======================================================= screen edge guards == */

static int sb_in_row(int row)
{
	return row >= 0 && row < g_sb.rows;
}

static int sb_clip_w(int col, int w)
{
	if (col < 0) { w += col; col = 0; }
	if (col >= g_sb.cols) return 0;
	if (col + w > g_sb.cols) w = g_sb.cols - col;
	return w > 0 ? w : 0;
}

static int sb_clip_h(int row, int h)
{
	if (row < 0) { h += row; row = 0; }
	if (row >= g_sb.rows) return 0;
	if (row + h > g_sb.rows) h = g_sb.rows - row;
	return h > 0 ? h : 0;
}

static void sb_fill(int row, int col, int width, char ch, const char *attrs)
{
	width = sb_clip_w(col, width);
	if (!sb_in_row(row) || width <= 0) return;
	for (int c = col; c < col + width; c++)
		sb_char(row, c, ch, attrs);
}

static void sb_str(int row, int col, int max_w, const char *s, const char *attrs)
{
	for (int i = 0; s && s[i] && i < max_w; i++)
		sb_char(row, col + i, s[i], attrs);
}

/* ======================================================= box-drawing chars == */
#define BX_TL  "\xe2\x94\x8c"   /* ┌ */
#define BX_TR  "\xe2\x94\x90"   /* ┐ */
#define BX_BL  "\xe2\x94\x94"   /* └ */
#define BX_BR  "\xe2\x94\x98"   /* ┘ */
#define BX_H   "\xe2\x94\x80"   /* ─ */
#define BX_V   "\xe2\x94\x82"   /* │ */
/* rounded corners (for launcher tiles) */
#define BX_RTL "\xe2\x95\xad"   /* ╭ */
#define BX_RTR "\xe2\x95\xae"   /* ╮ */
#define BX_RBL "\xe2\x95\xb0"   /* ╰ */
#define BX_RBR "\xe2\x95\xaf"   /* ╯ */

static void sb_hline(int row, int col, int width, const char *lc, const char *rc)
{
	width = sb_clip_w(col, width);
	if (!sb_in_row(row) || width <= 0) return;
	sb_utf8(row, col, lc, "");
	for (int c = col + 1; c < col + width - 1; c++)
		sb_utf8(row, c, BX_H, "");
	if (width > 1) sb_utf8(row, col + width - 1, rc, "");
}

/* horizontal rule with chosen corners + a uniform attribute (rounded tiles) */
static void sb_hline_a(int row, int col, int width, const char *lc,
                       const char *rc, const char *attr)
{
	width = sb_clip_w(col, width);
	if (!sb_in_row(row) || width <= 0) return;
	sb_utf8(row, col, lc, attr);
	for (int c = col + 1; c < col + width - 1; c++)
		sb_utf8(row, c, BX_H, attr);
	if (width > 1) sb_utf8(row, col + width - 1, rc, attr);
}

/* centre an ASCII string within [col, col+width) with a shared attribute */
static void sb_center(int row, int col, int width, const char *s, const char *attr)
{
	int n = 0; while (s && s[n]) n++;
	if (n > width) n = width;
	int off = (width - n) / 2;
	for (int i = 0; i < n; i++) sb_char(row, col + off + i, s[i], attr);
}

/* Relative layout: divide width into n equal segments (remainder distributed). */
static int ui_seg_x(int width, int n, int i) { return UI_SEG_X(width, n, i); }
static int ui_seg_w(int width, int n, int i) { return UI_SEG_W(width, n, i); }

/* =========================================== UiElem layout and rendering == */

static void render_elem(const UiElem *e, int row, int col, int width);

static int elem_height(const UiElem *e, int width)
{
	int h;
	switch (e->type) {
	case UI_HBAR:   return 1;
	case UI_METERS: return 1;
	case UI_HTOP_METERS: return 1;
	case UI_TEXT:   return 1;
	case UI_SPACER: return e->h > 0 ? e->h : 1;
	case UI_SEP:    return 1;
	case UI_CANVAS: return e->ch > 0 ? e->ch : 0;
	case UI_TILE:   return e->h > 0 ? e->h : UI_TILE_H;
	case UI_GRID: {
		int cols = e->n_cols > 0 ? e->n_cols : 1;
		int th = e->h > 0 ? e->h : UI_TILE_H;
		int total = (e->n_children + cols - 1) / cols;
		int vrows = e->ch > 0 ? e->ch : total;
		if (vrows > total) vrows = total;
		return vrows * th;
	}
	case UI_DIV:
		/* chrome: top-border + title + sep + [children] + sep + status + bottom.
		 * When .h is set, the box stretches to that height (edge-to-edge layout). */
		h = UI_DIV_CHROME;
		for (int i = 0; i < e->n_children; i++)
			h += elem_height(&e->children[i], width > 2 ? width - 2 : 0);
		if (e->h > h)
			h = e->h;
		return h;
	case UI_SCREEN:
	default: return 0;
	}
}

/* ── HBAR ───────────────────────────────────────────────────────────────── */

static const char *bar_fill_attr(const UiElem *e)
{
	static char ansi[16];
	if (!e->style.fg) {
		/* default: bright green */
		return (e->style.flags & UI_BOLD) ? "\033[1;32m" : "\033[0;32m";
	}
	/* Emit "\033[<bold>;fgm" using the stored fg code */
	int p = 0;
	ansi[p++] = '\033'; ansi[p++] = '[';
	if (e->style.flags & UI_BOLD) { ansi[p++] = '1'; ansi[p++] = ';'; }
	else                           { ansi[p++] = '0'; ansi[p++] = ';'; }
	unsigned fg = e->style.fg;
	if (fg >= 10) { ansi[p++] = (char)('0' + fg / 10); fg %= 10; }
	ansi[p++] = (char)('0' + fg);
	ansi[p++] = 'm'; ansi[p] = 0;
	return ansi;
}

static void render_hbar(const UiElem *e, int row, int col, int width)
{
	width = sb_clip_w(col, width);
	if (!sb_in_row(row) || width <= 0) return;
	const char *label = e->label ? e->label : "";
	const char *right = e->right ? e->right : "";
	int end = col + width;
	int c = col;

	if (c < end) sb_char(row, c++, ' ', "");

	for (int i = 0; label[i] && c < end; i++)
		sb_char(row, c++, label[i], "");

	if (c >= end) return;
	sb_char(row, c++, '[', "");

	int rlen = 0;
	while (right[rlen]) rlen++;
	int close = end - 1;
	if (close <= c) return;

	int fw = close - c - rlen;
	if (fw < 0) {
		rlen = close - c;
		if (rlen < 0) rlen = 0;
		fw = 0;
	}

	int fill = (e->max > 0 && fw > 0) ?
		(int)((long long)e->value * fw / e->max) : 0;
	if (fill < 0)   fill = 0;
	if (fill > fw)  fill = fw;

	const char *fa = bar_fill_attr(e);
	for (int i = 0; i < fill && c < close; i++) sb_char(row, c++, '|', fa);
	for (int i = fill; i < fw && c < close; i++) sb_char(row, c++, ' ', "");
	for (int i = 0; i < rlen && c < close; i++) sb_char(row, c++, right[i], fa);
	if (c < end) sb_char(row, c++, ']', "");
}

/* ── METERS (one htop-style row: CPU0 + APU1-3) ─────────────────────────── */

/*
 * The standard system meter: CPU0 utilisation + the three APU cores, all on a
 * single horizontal row (htop convention). Self-populating from the kernel's
 * idle accounting and the accelerator job counters, so every surface that wants
 * the meter just drops a {.type = UI_METERS} at the top of its tree.
 */
static void render_meters(int row, int col, int width)
{
	char right[UI_METERS_Q][12];
	UiElem bar;
	UiStyle style;
	static const char *labels[UI_METERS_Q] = { "CPU0", "CPU1", "CPU2", "CPU3" };
	unsigned tenths[UI_METERS_Q];

	width = sb_clip_w(col, width);
	if (!sb_in_row(row) || width <= 0) return;

	cpu_util_sample();
	tenths[0] = cpu_util_tenths();
	for (int i = 1; i < UI_METERS_Q; i++)
		tenths[i] = apu_util_tenths(i);

	for (int i = 0; i < UI_METERS_Q; i++) {
		ui_style_cpu_load(tenths[i], &style);
		ui_hbar_init_pct(&bar, right[i], (int)sizeof(right[i]),
		                 labels[i], tenths[i], &style);
		render_hbar(&bar, row, col + ui_seg_x(width, UI_METERS_Q, i),
		            ui_seg_w(width, UI_METERS_Q, i));
	}
}

/* htop monitor: CPU%, Mem pages, heap, APU1-3 on one horizontal row. */
static void render_htop_meters(int row, int col, int width)
{
	char rt[24];
	UiElem bar;
	int p;
	unsigned ft = (unsigned)pmm_total_pages(), ff = (unsigned)pmm_free_pages_count();
	unsigned ht = (unsigned)malloc_total_bytes(), hf = (unsigned)malloc_free_bytes();
	unsigned fused = ft - ff, hused = ht - hf;
	int seg = width / 6;
	if (seg < 6) seg = width;

	cpu_util_sample();
	{
		unsigned tenths = cpu_util_tenths();
		UiStyle style;
		ui_style_cpu_load(tenths, &style);
		ui_hbar_init_pct(&bar, rt, (int)sizeof(rt), "CPU", tenths, &style);
		render_hbar(&bar, row, col, seg);
	}
	if (seg == width) return;

	{
		unsigned u10 = fused * 10 / 256, t10 = ft * 10 / 256;
		p = 0; p = ui_utoa(u10 / 10, rt, (int)sizeof(rt)); rt[p++] = '.'; p += ui_utoa(u10 % 10, rt + p, (int)sizeof(rt) - p); rt[p++] = 'M'; rt[p++] = '/';
		p += ui_utoa(t10 / 10, rt + p, (int)sizeof(rt) - p); rt[p++] = '.'; p += ui_utoa(t10 % 10, rt + p, (int)sizeof(rt) - p); rt[p++] = 'M'; rt[p] = 0;
		ui_hbar_init(&bar, "Mem", rt, (int)fused, (int)(ft ? ft : 1), &ui_style_cpu_meter);
		render_hbar(&bar, row, col + seg, seg);
	}
	{
		static const UiStyle heap_style = { .fg = 33, .flags = UI_BOLD };
		p = 0; p = ui_utoa(hused / 1024, rt, (int)sizeof(rt)); rt[p++] = 'K'; rt[p++] = '/';
		p += ui_utoa(ht / 1024, rt + p, (int)sizeof(rt) - p); rt[p++] = 'K'; rt[p] = 0;
		ui_hbar_init(&bar, "Hp ", rt, (int)hused, (int)(ht ? ht : 1), &heap_style);
		render_hbar(&bar, row, col + seg * 2, seg);
	}
	{
		static const char *cpu_lbl[3] = { "CPU1", "CPU2", "CPU3" };
		for (int i = 0; i < 3; i++) {
			unsigned at = apu_util_tenths(i + 1);
			UiStyle style;
			ui_style_cpu_load(at, &style);
			ui_hbar_init_pct(&bar, rt, (int)sizeof(rt), cpu_lbl[i], at, &style);
			render_hbar(&bar, row, col + seg * (3 + i), seg);
		}
	}
}

static const char *elem_text_attr(const UiElem *e)
{
	static char ansi[24];
	int p = 0;
	if (!e->style.fg && !e->style.bg && !e->style.flags) return "";
	ansi[p++] = '\033'; ansi[p++] = '[';
	if (e->style.flags & UI_BOLD) { ansi[p++] = '1'; }
	else if (e->style.fg || e->style.bg) { ansi[p++] = '0'; }
	if (e->style.fg) {
		if (p > 2) ansi[p++] = ';';
		unsigned fg = e->style.fg;
		if (fg >= 10) { ansi[p++] = (char)('0' + fg / 10); fg %= 10; }
		ansi[p++] = (char)('0' + fg);
	}
	if (e->style.bg) {
		if (p > 2) ansi[p++] = ';';
		unsigned bg = e->style.bg;
		if (bg >= 10) { ansi[p++] = (char)('0' + bg / 10); bg %= 10; }
		ansi[p++] = (char)('0' + bg);
	}
	ansi[p++] = 'm'; ansi[p] = 0;
	return ansi;
}

/* ── DIV ────────────────────────────────────────────────────────────────── */

static void render_div(const UiElem *e, int row, int col, int width)
{
	if (row >= g_sb.rows) return;
	width = sb_clip_w(col, width);
	if (width <= 0) return;
	int inner_w = width > 2 ? width - 2 : 0;
	int r = row;
	int body_target = 0;
	int body_start;

	for (int i = 0; i < e->n_children; i++)
		body_target += elem_height(&e->children[i], inner_w);
	if (e->h > 0) {
		int want = e->h - UI_DIV_CHROME;
		if (want > body_target)
			body_target = want;
	}

	/* ┌──...──┐ — outer border flush with display left/right (col .. col+width-1) */
	sb_hline(r++, col, width, BX_TL, BX_TR);

	/* │ title (inverted)   │ */
	sb_utf8(r, col, BX_V, "");
	sb_fill(r, col + 1, inner_w, ' ', "\033[7m");
	if (e->title) sb_str(r, col + 1, inner_w, e->title, "\033[7m");
	if (width > 1) sb_utf8(r, col + width - 1, BX_V, "");
	r++;

	/* │─────────────────────│ separator */
	sb_utf8(r, col, BX_V, "");
	for (int c = col + 1; c < col + width - 1; c++) sb_utf8(r, c, BX_H, "");
	if (width > 1) sb_utf8(r, col + width - 1, BX_V, "");
	r++;

	/* children — │ border on each content row; pad to body_target when .h set */
	body_start = r;
	for (int i = 0; i < e->n_children; i++) {
		const UiElem *ch = &e->children[i];
		int ch_h = elem_height(ch, inner_w);
		if (r + ch_h > g_sb.rows)
			ch_h = g_sb.rows - r;
		if (ch_h <= 0) break;
		for (int cr = r; cr < r + ch_h && cr < g_sb.rows; cr++) {
			sb_utf8(cr, col, BX_V, "");
			if (width > 1) sb_utf8(cr, col + width - 1, BX_V, "");
		}
		render_elem(ch, r, col + 1, inner_w);
		r += ch_h;
	}
	while (r < body_start + body_target && r < g_sb.rows) {
		sb_utf8(r, col, BX_V, "");
		sb_fill(r, col + 1, inner_w, ' ', "");
		if (width > 1) sb_utf8(r, col + width - 1, BX_V, "");
		r++;
	}

	/* │─────────────────────│ separator */
	if (r >= g_sb.rows) return;
	sb_utf8(r, col, BX_V, "");
	for (int c = col + 1; c < col + width - 1; c++) sb_utf8(r, c, BX_H, "");
	if (width > 1) sb_utf8(r, col + width - 1, BX_V, "");
	r++;

	/* │ status text         │ */
	if (r >= g_sb.rows) return;
	sb_utf8(r, col, BX_V, "");
	sb_fill(r, col + 1, inner_w, ' ', "");
	if (e->status) sb_str(r, col + 1, inner_w, e->status, "");
	if (width > 1) sb_utf8(r, col + width - 1, BX_V, "");
	r++;

	/* └──...──┘ */
	if (r >= g_sb.rows) return;
	sb_hline(r, col, width, BX_BL, BX_BR);
}

/* ── CANVAS (APU-accelerated) ───────────────────────────────────────────── */

typedef struct {
	ScreenBuf          *sb;
	const char (*cells)[UI_CANVAS_WMAX];
	const unsigned char (*fg)[UI_CANVAS_WMAX];
	const unsigned char (*g3)[UI_CANVAS_WMAX][3];
	int                 sb_row;
	int                 canvas_row_start;
	int                 canvas_row_end;
	int                 canvas_w;
	int                 col_offset;
} CanvasBand;

static CanvasBand g_cbands[3];

static void cv_attr_fg(unsigned char code, unsigned char a[SBUF_ATTRS])
{
	int p = 0;

	if (!code) {
		a[0] = 0;
		return;
	}
	a[p++] = '\033';
	a[p++] = '[';
	a[p++] = '0';
	a[p++] = ';';
	if (code >= 100) {
		a[p++] = (unsigned char)('0' + code / 100);
		code = (unsigned char)(code % 100);
	}
	if (code >= 10)
		a[p++] = (unsigned char)('0' + code / 10);
	a[p++] = (unsigned char)('0' + (code % 10));
	a[p++] = 'm';
	a[p] = 0;
}

static int cv_cell_same(const ScreenCell *cur, const unsigned char g[3],
                        const unsigned char *attr)
{
	int i;

	for (i = 0; i < 3; i++)
		if (cur->g[i] != g[i])
			return 0;
	if (!attr || !attr[0])
		return cur->a[0] == 0;
	for (i = 0; attr[i] && i < SBUF_ATTRS - 1; i++)
		if (cur->a[i] != attr[i])
			return 0;
	return cur->a[i] == 0;
}

static void canvas_band_fn(void *arg)
{
	CanvasBand *b = arg;

	for (int r = b->canvas_row_start; r < b->canvas_row_end; r++) {
		for (int c = 0; c < b->canvas_w; c++) {
			int sr = b->sb_row + r;
			int sc = b->col_offset + c;
			unsigned char g[3];
			unsigned char a[SBUF_ATTRS];
			unsigned char fg = 0;
			const ScreenCell *cur = &b->sb->cell[sr][sc];
			ScreenCell *nxt = &b->sb->next[sr][sc];

			if (b->g3 && b->g3[r][c][0]) {
				g[0] = b->g3[r][c][0];
				g[1] = b->g3[r][c][1];
				g[2] = b->g3[r][c][2];
			} else {
				g[0] = (unsigned char)b->cells[r][c];
				g[1] = 0;
				g[2] = 0;
			}
			if (b->fg)
				fg = b->fg[r][c];
			cv_attr_fg(fg, a);

			if (cv_cell_same(cur, g, a)) {
				*nxt = *cur;
				continue;
			}
			screenbuf_set(b->sb, sr, sc, g, a);
		}
	}
}

static void render_canvas(const UiElem *e, int row, int col, int width)
{
	int w = (e->cw > width) ? width : e->cw;
	int h = e->ch;
	if (!e->cells || h <= 0 || w <= 0) return;

	/* Seed next[] from current[] so static canvas cells skip the UART diff. */
	screenbuf_copy_rect(&g_sb, row, col, w, h);

	{
		int band = (h + 2) / 3;
		ApuJob cb_jobs[3];
		int nj = 0;
		for (int i = 0; i < 3; i++) {
			int rs = i * band;
			int re = rs + band;
			if (re > h) re = h;
			g_cbands[i].sb               = &g_sb;
			g_cbands[i].cells            = e->cells;
			g_cbands[i].fg               = e->cv_fg;
			g_cbands[i].g3               = e->cv_g3;
			g_cbands[i].sb_row           = row;
			g_cbands[i].canvas_row_start = rs;
			g_cbands[i].canvas_row_end   = re;
			g_cbands[i].canvas_w         = w;
			g_cbands[i].col_offset       = col;
			if (rs < re) {
				cb_jobs[nj].fn  = canvas_band_fn;
				cb_jobs[nj].arg = &g_cbands[i];
				nj++;
			}
		}
		if (nj > 0)
			APUBatch(cb_jobs, nj);
	}
}

/* ── TILE / GRID (app launcher) ─────────────────────────────────────────── */

/* One rounded-corner square: icon line over a name line. When `selected`, the
 * border is bright-cyan bold and the interior is inverse-video. */
static void render_tile(const UiElem *e, int row, int col, int width, int height,
                      int selected)
{
	width = sb_clip_w(col, width);
	height = sb_clip_h(row, height);
	if (width < UI_TILE_MIN_W || height < UI_TILE_MIN_H) return;
	const char *bd = selected ? "\033[1;36m" : "";   /* border attr */
	const char *in = selected ? "\033[7m"    : "";   /* interior attr */
	int inner = width - 2;

	sb_hline_a(row, col, width, BX_RTL, BX_RTR, bd);            /* ╭──╮ */

	/* icon row */
	sb_utf8(row + 1, col, BX_V, bd);
	sb_fill(row + 1, col + 1, inner, ' ', in);
	if (e->text) sb_center(row + 1, col + 1, inner, e->text, in);
	sb_utf8(row + 1, col + width - 1, BX_V, bd);

	/* middle padding rows (taller tiles) */
	for (int mr = 2; mr < height - 2; mr++) {
		sb_utf8(row + mr, col, BX_V, bd);
		sb_fill(row + mr, col + 1, inner, ' ', in);
		sb_utf8(row + mr, col + width - 1, BX_V, bd);
	}

	/* name row */
	{
		int nr = height - 2;
		sb_utf8(row + nr, col, BX_V, bd);
		sb_fill(row + nr, col + 1, inner, ' ', in);
		if (e->title) sb_center(row + nr, col + 1, inner, e->title, in);
		sb_utf8(row + nr, col + width - 1, BX_V, bd);
	}

	sb_hline_a(row + height - 1, col, width, BX_RBL, BX_RBR, bd);        /* ╰──╯ */
}

static void render_grid(const UiElem *e, int row, int col, int width)
{
	width = sb_clip_w(col, width);
	if (width <= 0) return;
	int cols = e->n_cols > 0 ? e->n_cols : 1;
	int th = e->h > 0 ? e->h : UI_TILE_H;
	int total = (e->n_children + cols - 1) / cols;
	int vrows = e->ch > 0 ? e->ch : total;
	if (vrows > total) vrows = total;
	if (vrows < 1) vrows = 1;

	int npages = (total + vrows - 1) / vrows;
	if (npages < 1) npages = 1;
	int page = e->page;
	if (page < 0) page = 0;
	if (page >= npages) page = npages - 1;
	int page_row = page * vrows;

	int view_h = sb_clip_h(row, vrows * th);
	int max_dr = view_h / th;
	if (max_dr < 1 && view_h > 0) max_dr = 1;
	if (max_dr > vrows) max_dr = vrows;

	for (int i = 0; i < e->n_children; i++) {
		int gr = i / cols, gc = i % cols;
		if (gr < page_row || gr >= page_row + vrows) continue;
		int dr = gr - page_row;
		if (dr >= max_dr) continue;
		int seg_x = col + ui_seg_x(width, cols, gc);
		int seg_w = ui_seg_w(width, cols, gc);
		if (seg_w <= 0) continue;
		int tw = seg_w;
		int tile_h = tw / UI_TILE_ASPECT;
		if (tile_h > th) tile_h = th;
		tile_h = sb_clip_h(row + dr * th, tile_h);
		if (tw < UI_TILE_MIN_W || tile_h < UI_TILE_MIN_H) continue;
		render_tile(&e->children[i],
		            row + dr * th,
		            seg_x, tw, tile_h,
		            i == e->sel);
	}
}

static int screen_has_meters(const UiElem *e)
{
	for (int i = 0; i < e->n_children; i++)
		if (e->children[i].type == UI_METERS) return 1;
	return 0;
}

/* ── generic dispatch ───────────────────────────────────────────────────── */

static void render_elem(const UiElem *e, int row, int col, int width)
{
	if (!e) return;
	switch (e->type) {
	case UI_SCREEN:
		row = 0; col = 0; width = g_sb.cols;
		/* Universal CPU bar (CPU0 + CPU1-3, UI_SEG_* quarters) on every GUI screen. */
		if (!screen_has_meters(e)) {
			if (row < g_sb.rows) {
				int mw = sb_clip_w(col, g_sb.cols);
				if (mw > 0)
					render_meters(row, col, mw);
				row += UI_SCREEN_METER_ROWS;
			}
		}
		for (int i = 0; i < e->n_children; i++) {
			if (row >= g_sb.rows) break;
			width = sb_clip_w(col, g_sb.cols - col);
			if (width <= 0) break;
			int h = elem_height(&e->children[i], width);
			if (row + h > g_sb.rows)
				h = g_sb.rows - row;
			if (h <= 0) break;
			render_elem(&e->children[i], row, col, width);
			row += elem_height(&e->children[i], width);
		}
		break;
	case UI_DIV:    render_div   (e, row, col, width); break;
	case UI_HBAR:   render_hbar  (e, row, col, width); break;
	case UI_METERS: render_meters(row, col, width); break;
	case UI_HTOP_METERS: render_htop_meters(row, col, width); break;
	case UI_TEXT: {
		const char *attr = elem_text_attr(e);
		sb_fill(row, col, width, ' ', attr);
		if (e->text) {
			if (e->style.align == UI_CENTER)
				sb_center(row, col, width, e->text, attr);
			else
				sb_str(row, col, width, e->text, attr);
		}
		break;
	}
	case UI_SPACER:
		for (int i = 0; i < (e->h > 0 ? e->h : 1); i++)
			sb_fill(row + i, col, width, ' ', "");
		break;
	case UI_SEP:
		for (int c = col; c < col + width; c++) sb_utf8(row, c, BX_H, "");
		break;
	case UI_CANVAS: render_canvas(e, row, col, width); break;
	case UI_GRID:   render_grid  (e, row, col, width); break;
	case UI_TILE: {
		int th = e->h > 0 ? e->h : UI_TILE_H;
		int tw = width;
		if (tw / UI_TILE_ASPECT < th) th = tw / UI_TILE_ASPECT;
		if (tw < UI_TILE_MIN_W || th < UI_TILE_MIN_H) break;
		render_tile(e, row, col, tw, th, 0);
		break;
	}
	}
}

static void handle_render_tree(UiElem *root, int flags)
{
	g_cursor_visible = 0;
	/* Frame build: reset next[], paint tree into next[], diff next[] vs cell[]. */
	g_tview = 0;
	if (flags & DISP_FLAG_CLEAR)
		screenbuf_clear_buf(&g_sb);
	else
		screenbuf_clear_next(&g_sb);
	render_elem(root, 0, 0, g_sb.cols);
	disp_render_now();
}

void disp_tetris_render_shell(const char *status)
{
	UiElem body = { .type = UI_SPACER, .h = ui_screen_body_h(g_sb.rows) };
	UiElem frame = {
		.type       = UI_DIV,
		.title      = "TETRIS",
		.status     = status,
		.h          = UI_APP_FRAME_H(g_sb.rows),
		.children   = &body,
		.n_children = 1,
	};
	UiElem screen = { .type = UI_SCREEN, .children = &frame, .n_children = 1 };
	handle_render_tree(&screen, DISP_FLAG_CLEAR);
}

/* Blank the terminal and bring the server's cell model in sync with it. */
static void handle_clear(void)
{
	console_route_bypass(1);
	disp_emit_str("\033[0m\033[2J\033[H\033[?25l");
	console_route_bypass(0);
	screenbuf_sync_blank(&g_sb);
	g_term.row = 0;
	g_term.col = 0;
	g_term.attrs[0] = 0;
	term_reset_parser();
	g_tview = 0;    /* snap to live; scrollback history is preserved across screens */
}

/* ========================================= legacy ANSI-parsing (WRITE path) */

/* Push one line into the scrollback history ring (oldest dropped when full). */
static void thist_push_row(const ScreenCell *row)
{
	int slot;
	if (g_thist_count < THIST_LINES) {
		slot = (g_thist_head + g_thist_count) % THIST_LINES;
		g_thist_count++;
	} else {
		slot = g_thist_head;
		g_thist_head = (g_thist_head + 1) % THIST_LINES;
	}
	for (int c = 0; c < g_sb.cols; c++) g_thist[slot][c] = row[c];
}

/* Emit a real one-line scroll to the physical terminal, confined to rows
 * 1..g_sb.rows via a DECSTBM scroll region so it works at any terminal height.
 * Sequence: set region, go to its bottom, LF (scrolls the region up), reset. */
static int term_shell_col0(void)
{
	return g_shell_r0 >= 0 ? 1 : 0;
}

static int term_shell_col_last(void)
{
	return g_sb.cols > 2 ? g_sb.cols - 2 : g_sb.cols - 1;
}

static int term_scroll_bottom(void)
{
	if (g_shell_r1 >= 0)
		return g_shell_r1;
	return g_sb.rows - 1;
}

static void term_emit_scroll(void)
{
	char s[32];
	int p = 0;
	int top = (g_shell_r0 >= 0 ? g_shell_r0 : 0) + 1;
	int bot = term_scroll_bottom() + 1;

	s[p++] = '\033'; s[p++] = '[';
	if (top >= 10) s[p++] = (char)('0' + top / 10);
	s[p++] = (char)('0' + top % 10);
	s[p++] = ';';
	if (bot >= 10) s[p++] = (char)('0' + bot / 10);
	s[p++] = (char)('0' + bot % 10);
	s[p++] = 'r';
	s[p++] = '\033'; s[p++] = '[';
	if (bot >= 10) s[p++] = (char)('0' + bot / 10);
	s[p++] = (char)('0' + bot % 10);
	s[p++] = ';'; s[p++] = '1'; s[p++] = 'H';
	s[p++] = '\n';
	s[p++] = '\033'; s[p++] = '['; s[p++] = 'r';
	s[p] = 0;
	console_route_bypass(1);
	disp_emit_str(s);
	console_route_bypass(0);
}

/* Advance the cursor down one row; scroll the buffer up when past the bottom so
 * the WRITE path behaves like a real terminal (content scrolls, cursor pins).
 * The line that scrolls off the top is preserved in the scrollback history.
 * Only when at the live bottom do we use a native scroll: shift both buffers up
 * and scroll the physical terminal so the diff repaints just the new line. */
static void term_linefeed(void)
{
	int r0 = g_shell_r0 >= 0 ? g_shell_r0 : 0;
	int bot = term_scroll_bottom();

	g_term.row++;
	if (g_term.row > bot) {
		thist_push_row(g_sb.next[r0]);
		if (g_shell_r0 >= 0) {
			int c0 = term_shell_col0();
			int c1 = term_shell_col_last();
			screenbuf_scroll_span_cols_up(&g_sb, r0, bot, c0, c1);
			if (g_tview == 0) {
				screenbuf_scroll_span_cols_cell_up(&g_sb, r0, bot, c0, c1);
				term_emit_scroll();
			}
		} else {
			screenbuf_scroll_up(&g_sb, 1);
			if (g_tview == 0) {
				screenbuf_scroll_cell_up(&g_sb, 1);
				term_emit_scroll();
			}
		}
		g_term.row = bot;
	}
}

/* Snap back to the live screen (discard any scrollback offset). */
static void term_view_live(void)
{
	if (g_tview == 0) return;
	for (int r = 0; r < g_sb.rows; r++)
		for (int c = 0; c < g_sb.cols; c++)
			g_sb.next[r][c] = g_tsave[r][c];
	g_tview = 0;
}

/* Compose the scrolled-back viewport into next[] from history + the saved live
 * screen, so screenbuf_render() diffs and emits only what changed. */
static void term_compose_view(void)
{
	for (int i = 0; i < g_sb.rows; i++) {
		int idx = g_thist_count - g_tview + i;   /* index into history++live   */
		for (int c = 0; c < g_sb.cols; c++) {
			if (idx < 0) {
				g_sb.next[i][c].g[0] = ' ';
				g_sb.next[i][c].g[1] = 0; g_sb.next[i][c].g[2] = 0;
				g_sb.next[i][c].a[0] = 0;
			} else if (idx < g_thist_count) {
				g_sb.next[i][c] = g_thist[(g_thist_head + idx) % THIST_LINES][c];
			} else {
				g_sb.next[i][c] = g_tsave[idx - g_thist_count][c];
			}
		}
	}
}

/* Scroll the terminal viewport by `delta` lines (+ = older, - = newer). */
static void term_scroll(int delta)
{
	int old = g_tview;
	int nv = old + delta;
	if (nv < 0) nv = 0;
	if (nv > g_thist_count) nv = g_thist_count;
	if (nv == old) return;

	if (old == 0) {   /* leaving live: snapshot the live screen to restore later */
		for (int r = 0; r < g_sb.rows; r++)
			for (int c = 0; c < g_sb.cols; c++)
				g_tsave[r][c] = g_sb.next[r][c];
	}
	g_tview = nv;
	if (g_tview == 0) term_view_live();
	else              term_compose_view();
	disp_render_now();
}

static void term_put_glyph(unsigned char ch)
{
	unsigned char g[3];
	g[0] = ch; g[1] = 0; g[2] = 0;
	screenbuf_set(&g_sb, g_term.row, g_term.col, g, g_term.attrs);
	g_term.col++;
	if (g_shell_r0 >= 0) {
		if (g_term.col > term_shell_col_last()) {
			g_term.col = term_shell_col0();
			term_linefeed();
		}
	} else if (g_term.col >= g_sb.cols) {
		g_term.col = 0;
		term_linefeed();
	}
}

static void term_put_utf8(unsigned char b0, unsigned char b1, unsigned char b2)
{
	unsigned char g[3];
	g[0] = b0; g[1] = b1; g[2] = b2;
	screenbuf_set(&g_sb, g_term.row, g_term.col, g, g_term.attrs);
	g_term.col++;
	if (g_shell_r0 >= 0) {
		if (g_term.col > term_shell_col_last()) {
			g_term.col = term_shell_col0();
			term_linefeed();
		}
	} else if (g_term.col >= g_sb.cols) {
		g_term.col = 0;
		term_linefeed();
	}
}

static void term_set_attrs(const char *seq, int len)
{
	int i;
	if (len >= SBUF_ATTRS) len = SBUF_ATTRS - 1;
	for (i = 0; i < len; i++) g_term.attrs[i] = (unsigned char)seq[i];
	g_term.attrs[i] = 0;
}

static int readline_min_col(void)
{
	if (g_term.row == g_readline_start_row)
		return g_input_min_col;
	return term_shell_col0();
}

static void term_erase_input_char(void)
{
	int min_col = readline_min_col();

	if (g_shell_r0 >= 0 && min_col < term_shell_col0())
		min_col = term_shell_col0();
	if (g_term.col <= min_col) {
		if (g_term.row <= g_readline_start_row)
			return;
		g_term.row--;
		g_term.col = term_shell_col_last();
	} else {
		g_term.col--;
	}
	sb_char(g_term.row, g_term.col, ' ', "");
	g_readline_row = g_term.row;
	g_readline_col = g_term.col;
	disp_render_now();
}

static void term_cursor_goto(int row1, int col1)
{
	g_term.row = row1 - 1;
	g_term.col = col1 - 1;
	if (g_term.row < 0) g_term.row = 0;
	if (g_term.col < 0) g_term.col = 0;
	if (g_term.row >= g_sb.rows) g_term.row = g_sb.rows - 1;
	if (g_term.col >= g_sb.cols) g_term.col = g_sb.cols - 1;
}

static void term_readline_echo(unsigned char ch)
{
	if (g_readline_row < 0)
		return;
	if (g_term.row != g_readline_row || g_term.col != g_readline_col)
		term_cursor_goto(g_readline_row + 1, g_readline_col + 1);
	if (ch >= 32)
		term_put_glyph(ch);
	g_readline_row = g_term.row;
	g_readline_col = g_term.col;
}

static void term_feed(const char *data, int len)
{
	char sgr[SBUF_ATTRS];
	int sgr_n;
	int rendered = 0;

	term_view_live();   /* fresh output snaps the viewport back to the bottom */

	for (int i = 0; i < len; i++) {
		unsigned char ch = (unsigned char)data[i];

		/* Assemble a multi-byte UTF-8 glyph — may span chunk boundaries. */
		if (g_term.st == ST_NORMAL && g_term.upend_n > 0) {
			if (ch >= 0x80 && ch < 0xC0) {            /* continuation byte */
				if (g_term.upend_n < 3) g_term.upend[g_term.upend_n] = ch;
				g_term.upend_n++;
				if (g_term.upend_n >= g_term.upend_need) {
					if (g_term.upend_need >= 4)
						term_put_utf8(0xEF, 0xBF, 0xBD);  /* U+FFFD: 4-byte won't fit a 3-byte cell */
					else
						term_put_utf8(g_term.upend[0],
						              g_term.upend_n > 1 ? g_term.upend[1] : 0,
						              g_term.upend_n > 2 ? g_term.upend[2] : 0);
					g_term.upend_n = 0; g_term.upend_need = 0;
				}
				continue;
			}
			g_term.upend_n = 0; g_term.upend_need = 0;  /* malformed: drop, reprocess ch */
		}

		switch (g_term.st) {
		case ST_NORMAL:
			if (ch == '\033') { g_term.st = ST_ESC; break; }
			if (ch == '\r')   { g_term.col = term_shell_col0(); break; }
			if (ch == '\n')   { g_term.col = term_shell_col0(); term_linefeed(); break; }
			if (ch == '\t')   {
				g_term.col = (g_term.col + 8) & ~7;
				if (g_term.col > term_shell_col_last())
					g_term.col = term_shell_col_last();
				if (g_term.col < term_shell_col0())
					g_term.col = term_shell_col0();
				break;
			}
			if (ch == '\b') {
				int min_col = g_input_min_col;
				if (g_shell_r0 >= 0 && min_col < term_shell_col0())
					min_col = term_shell_col0();
				if (g_term.col > min_col) {
					g_term.col--;
					sb_char(g_term.row, g_term.col, ' ', "");
				}
				break;
			}
			if (ch >= 0xC0) {                          /* UTF-8 lead byte */
				g_term.upend[0] = ch; g_term.upend_n = 1;
				g_term.upend_need = (ch >= 0xF0) ? 4 : (ch >= 0xE0) ? 3 : 2;
				break;
			}
			if (ch >= 0x80) break;                     /* stray continuation: ignore */
			if (ch >= 32)   { term_put_glyph(ch); break; }
			break;                                      /* other control: ignore */

		case ST_ESC:
			if (ch == '[') { g_term.param_n = 0; g_term.cur_param = 0; g_term.st = ST_CSI; }
			else if (ch == 'H') { g_term.row = 0; g_term.col = 0; g_term.st = ST_NORMAL; }
			else g_term.st = ST_NORMAL;
			break;

		case ST_CSI:
			if (ch >= '0' && ch <= '9') {
				g_term.cur_param = g_term.cur_param * 10 + (ch - '0'); break;
			}
			if (ch == ';') {
				if (g_term.param_n < 6) g_term.params[g_term.param_n++] = g_term.cur_param;
				g_term.cur_param = 0; break;
			}
			if (ch == '?') break;   /* skip private-mode prefix */

			if (g_term.param_n < 6) g_term.params[g_term.param_n++] = g_term.cur_param;

			if (ch == 'H' || ch == 'f') {
				term_cursor_goto(g_term.params[0] > 0 ? g_term.params[0] : 1,
				                 (g_term.param_n > 1 && g_term.params[1] > 0) ? g_term.params[1] : 1);
			} else if (ch == 'J') {
				if (g_term.params[0] == 2 || (g_term.param_n <= 1 && g_term.params[0] == 0))
					screenbuf_clear_buf(&g_sb);
			} else if (ch == 'K') {
				screenbuf_clear_eol(&g_sb, g_term.row, g_term.col);
			} else if (ch == 'm') {
				sgr_n = 0;
				sgr[sgr_n++] = '\033'; sgr[sgr_n++] = '[';
				for (int p = 0; p < g_term.param_n; p++) {
					if (p && sgr_n < SBUF_ATTRS - 1) sgr[sgr_n++] = ';';
					int v = g_term.params[p]; char d[6]; int nd = 0;
					if (v == 0) d[nd++] = '0';
					while (v) { d[nd++] = (char)('0' + v % 10); v /= 10; }
					while (nd && sgr_n < SBUF_ATTRS - 1) sgr[sgr_n++] = d[--nd];
				}
				if (sgr_n < SBUF_ATTRS - 1) sgr[sgr_n++] = 'm';
				sgr[sgr_n] = 0;
				term_set_attrs(sgr, sgr_n);
			} else if (ch == 'h' || ch == 'l') {
				if (g_term.params[0] == 2026) {
					if (ch == 'h') g_term.sync = 1;
					else { g_term.sync = 0; disp_render_now(); rendered = 1; }
				} else if (g_term.params[0] == 25) {
					console_route_bypass(1);
					disp_emit_str(ch == 'h' ? "\033[?25h" : "\033[?25l");
					console_route_bypass(0);
				}
			}
			g_term.param_n = 0; g_term.cur_param = 0; g_term.st = ST_NORMAL;
			break;

		default: g_term.st = ST_NORMAL; break;
		}
	}

	if (!g_term.sync && !rendered)
		disp_render_now();
}

static void ds_clamp_size(int *rows, int *cols)
{
	if (*cols < 20)
		*cols = 20;
	if (*cols > 300)
		*cols = 300;
	if (*rows < 6)
		*rows = 6;
	if (*rows > 100)
		*rows = 100;
}

static void ds_apply_size(int rows, int cols)
{
	ds_clamp_size(&rows, &cols);
	screenbuf_set_size(&g_sb, rows, cols);
	g_display_rows = g_sb.rows;
	g_display_cols = g_sb.cols;
}

static void ds_set_manual_size(int rows, int cols)
{
	g_display_manual = 1;
	ds_apply_size(rows, cols);
	ds_screen_cols_write(cols);
}

static void ds_query_terminal_size(int *rows, int *cols);

static void ds_set_autofit(void)
{
	g_display_manual = 0;
	DiskRm(DS_SCREEN_COLS_PATH);
	ds_query_terminal_size(&g_display_rows, &g_display_cols);
}

static void ds_size_reply(DispSizeReply *sz)
{
	sz->rows   = g_display_rows;
	sz->cols   = g_display_cols;
	sz->manual = g_display_manual;
}

/* Query physical terminal dimensions (CPR); resize buffer without clearing. */
static void ds_query_terminal_size(int *rows, int *cols)
{
	int clock = ClockServerTid();
	int rr = 0, cc = 0, stage = 0;

	if (g_display_manual) {
		if (rows)
			*rows = g_display_rows;
		if (cols)
			*cols = g_display_cols;
		return;
	}

	if (rows)
		*rows = g_display_rows;
	if (cols)
		*cols = g_display_cols;

	console_route_bypass(1);
	disp_emit_str("\033[999;999H\033[6n");
	console_route_bypass(0);

	for (int t = 0; t < 120; t++) {
		if (!uart_rxc(CONSOLE)) {
			if (clock >= 0) Delay(clock, 1);
			continue;
		}
		unsigned char ch = (unsigned char)uart_getc(CONSOLE);
		switch (stage) {
		case 0: if (ch == 0x1b) stage = 1; break;
		case 1: stage = (ch == '[') ? 2 : 0; break;
		case 2:
			if (ch >= '0' && ch <= '9') rr = rr * 10 + (ch - '0');
			else if (ch == ';') stage = 3;
			else stage = 0;
			break;
		case 3:
			if (ch >= '0' && ch <= '9') cc = cc * 10 + (ch - '0');
			else if (ch == 'R' && rr > 0 && cc > 0) {
				ds_apply_size(rr, cc);
				g_term.row = g_display_rows - 1;
				g_term.col = g_display_cols - 1;
				if (g_term.row >= g_sb.rows)
					g_term.row = g_sb.rows - 1;
				if (g_term.col >= g_sb.cols)
					g_term.col = g_sb.cols - 1;
				if (rows)
					*rows = g_display_rows;
				if (cols)
					*cols = g_display_cols;
				return;
			} else stage = 0;
			break;
		}
	}
}

static int ds_wait_key(void)
{
	int clock = ClockServerTid();
	while (!uart_rxc(CONSOLE)) {
		if (clock >= 0) Delay(clock, 1);
	}
	return (int)(unsigned char)uart_getc(CONSOLE);
}

/* ================================================================ server == */

void display_server_entry(void)
{
	int tid;
	DispMsg msg;
	int reply;

	display_server_tid = MyTid();
	RegisterAs(DISPLAY_SERVER_NAME);
	/* Lock design size from boot — apps render 37×27 until `screen auto`. */
	g_display_manual = 1;
	g_display_rows   = UI_DESIGN_SCREEN_ROWS;
	g_display_cols   = UI_DESIGN_SCREEN_COLS;
	screenbuf_init(&g_sb, g_display_rows, g_display_cols);
	g_term.row = 0;
	g_term.col = 0;
	g_term.attrs[0] = 0;
	term_reset_parser();
	g_thist_head = 0;
	g_thist_count = 0;
	g_tview = 0;
	g_cursor_visible = 0;
	g_input_min_col = 0;
	g_shell_r0 = -1;
	g_shell_r1 = -1;
	g_readline_row = -1;
	g_readline_col = 0;
	g_readline_start_row = 0;
	display_client_register_route();

	for (;;) {
		Receive(&tid, (char *)&msg, (int)sizeof(msg));

		switch (msg.type) {

		case DISP_MSG_WRITE:
			if (msg.len > 0 && msg.len <= DISP_MSG_CHUNK)
				term_feed(msg.data, msg.len);
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_RENDER:
			disp_render_now();
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_RENDER_TREE:
			if (msg.root) handle_render_tree((UiElem *)msg.root, msg.flags);
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_CLEAR:
			handle_clear();
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_SCROLL:
			term_scroll(msg.len);   /* msg.len carries the signed line delta */
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_GET_SIZE: {
			DispSizeReply sz;
			ds_size_reply(&sz);
			Reply(tid, (const char *)&sz, (int)sizeof(sz));
			break;
		}

		case DISP_MSG_QUERY_TERMINAL: {
			DispSizeReply sz;
			if (!g_display_manual)
				ds_query_terminal_size(&sz.rows, &sz.cols);
			ds_size_reply(&sz);
			Reply(tid, (const char *)&sz, (int)sizeof(sz));
			break;
		}

		case DISP_MSG_SET_SIZE: {
			DispSizeReply sz;
			ds_set_manual_size(msg.len, msg.flags);
			ds_size_reply(&sz);
			Reply(tid, (const char *)&sz, (int)sizeof(sz));
			break;
		}

		case DISP_MSG_SCREEN_AUTO: {
			DispSizeReply sz;
			ds_set_autofit();
			ds_size_reply(&sz);
			Reply(tid, (const char *)&sz, (int)sizeof(sz));
			break;
		}

		case DISP_MSG_WAIT_KEY: {
			int key = ds_wait_key();
			Reply(tid, (const char *)&key, (int)sizeof(key));
			break;
		}

		case DISP_MSG_PUT_CELL: {
			unsigned char g[3];
			unsigned char a[SBUF_ATTRS];
			int ai = 0;
			g[0] = (unsigned char)msg.data[0];
			g[1] = 0;
			g[2] = 0;
			for (int j = 1; msg.data[j] && ai < SBUF_ATTRS - 1; j++)
				a[ai++] = (unsigned char)msg.data[j];
			a[ai] = 0;
			screenbuf_set(&g_sb, msg.flags, msg.len, g, a);
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;
		}

		case DISP_MSG_FLUSH:
			disp_render_now();
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_CURSOR:
			g_cursor_visible = msg.len ? 1 : 0;
			console_route_bypass(1);
			disp_cursor_apply();
			console_route_bypass(0);
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_READLINE_START:
			g_input_min_col = g_term.col;
			if (g_shell_r0 >= 0 && g_input_min_col < term_shell_col0())
				g_input_min_col = term_shell_col0();
			g_readline_start_row = g_term.row;
			g_readline_row = g_term.row;
			g_readline_col = g_term.col;
			g_cursor_visible = 1;
			console_route_bypass(1);
			disp_cursor_apply();
			console_route_bypass(0);
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_READLINE_ECHO:
			term_readline_echo((unsigned char)msg.data[0]);
			disp_render_now();
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_READLINE_END:
			g_readline_row = -1;
			g_readline_start_row = 0;
			g_cursor_visible = 0;
			console_route_bypass(1);
			disp_cursor_apply();
			console_route_bypass(0);
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_GOTO_CURSOR:
			term_cursor_goto(msg.len + 1, msg.flags + 1);
			g_readline_col = g_term.col;
			console_route_bypass(1);
			term_emit_cursor_pos();
			disp_cursor_apply();
			console_route_bypass(0);
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_GET_CURSOR: {
			DispCursorReply cr;
			cr.row     = g_term.row;
			cr.col     = g_term.col;
			cr.min_col = g_input_min_col;
			Reply(tid, (char *)&cr, (int)sizeof(cr));
			break;
		}

		case DISP_MSG_ERASE_CHAR:
			term_erase_input_char();
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_SHELL_REGION:
			g_shell_r0 = msg.len;
			g_shell_r1 = msg.flags;
			if (g_shell_r1 < g_shell_r0)
				g_shell_r0 = g_shell_r1 = -1;
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_TETRIS_BEGIN:
			disp_tetris_begin();
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_TETRIS_UPDATE:
			if (msg.root)
				disp_tetris_update((const DispTetrisState *)msg.root);
			disp_render_now();
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		case DISP_MSG_TETRIS_END:
			disp_tetris_end();
			reply = 0;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;

		default:
			reply = -1;
			Reply(tid, (char *)&reply, (int)sizeof(reply));
			break;
		}
	}
}

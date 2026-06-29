#ifndef UI_ELEM_H
#define UI_ELEM_H

#include "config.h"   /* UI_CANVAS_WMAX — canvas backing array width */

/*
 * HTML-inspired declarative UI element tree.
 *
 * Build a tree of UiElem nodes on the stack (or static storage), then hand
 * it to the display server with display_render_tree(&root).  The display
 * server owns all rendering — including APU-backed canvas fill — so the
 * calling task never writes directly to the terminal.
 *
 * Tag vocabulary (analogous to HTML tags)
 * ──────────────────────────────────────────────────────────────────────────
 *  UI_SCREEN  root element; children stack top→down filling the terminal
 *             (UI_METERS is auto-prepended when absent — universal CPU bar row)
 *  UI_DIV     bordered box: ┌title─┐ / │children│ / │status│ / └──┘
 *  UI_HBAR    horizontal meter bar (like htop)   " LABEL[||||   right]"
 *  UI_TEXT    single padded text line
 *  UI_SEP     full-width horizontal rule  ─────────────────
 *  UI_CANVAS  raw character grid rendered with APU acceleration
 * ──────────────────────────────────────────────────────────────────────────
 *
 * Style (analogous to CSS properties)
 * ──────────────────────────────────────────────────────────────────────────
 *  fg    ANSI basic-color index 30-37/90-97 (0 = terminal default)
 *  bg    ANSI basic-color index 40-47       (0 = terminal default)
 *  flags UI_BOLD | UI_DIM | UI_INVERSE
 *  align UI_LEFT | UI_CENTER | UI_RIGHT
 * ──────────────────────────────────────────────────────────────────────────
 *
 * C polymorphism: UiElem uses flat named fields (no union) so any field can
 * be set with a plain designated initialiser.  Unused fields for a given
 * type are ignored by the renderer.
 *
 * Example (two-player Pong):
 *
 *   UiElem bars[4];
 *   char pct[4][12];
 *   ui_hbar_init_pct(&bars[0], pct[0], sizeof pct[0], "CPU0", cpu0_tenths, &ui_style_cpu_meter);
 *   ...
 *   UiElem canvas_elem = {
 *       .type = UI_CANVAS, .cells = cv.cells, .cw = cv.w, .ch = cv.h
 *   };
 *   UiElem div_ch[1] = { canvas_elem };
 *   UiElem game_div = {
 *       .type = UI_DIV, .title = "Pong 2P", .status = status_str,
 *       .children = div_ch, .n_children = 1
 *   };
 *   UiElem scr_ch[5] = { bars[0], bars[1], bars[2], bars[3], game_div };
 *   UiElem screen = { .type = UI_SCREEN, .children = scr_ch, .n_children = 5 };
 *   display_render_tree(&screen);
 */

typedef enum {
    UI_SCREEN = 0,
    UI_DIV,
    UI_HBAR,
    UI_TEXT,
    UI_SEP,
    UI_CANVAS,
    UI_GRID,    /* app-launcher: row-major grid of UI_TILE children            */
    UI_TILE,    /* rounded-corner square: an icon line over a name line         */
    UI_METERS,  /* one htop-style row: CPU0 + CPU1-3 meters (server-populated)  */
    UI_HTOP_METERS, /* htop monitor row: CPU%, Mem, Heap, CPU1-3 (server-populated) */
    UI_SPACER,  /* blank rows; height in .h                               */
} UiElemType;

/* Square launcher tile: terminal cells ~2:1 w:h → box is W cols × H rows, W = 2×H. */
#define UI_TILE_ASPECT   2
#define UI_TILE_H        4   /* default height when UI_GRID.h is 0 */
#define UI_TILE_MIN_H    4   /* icon row + name row + top/bottom borders      */
#define UI_TILE_MIN_W    (UI_TILE_MIN_H * UI_TILE_ASPECT)
#define UI_TILE_W        (UI_TILE_H * UI_TILE_ASPECT)

/* UI_METERS splits the row into this many equal-width segments (CPU0 + CPU1-3). */
#define UI_METERS_Q         4
#define UI_SCREEN_METER_ROWS 1   /* auto-prepended on UI_SCREEN when no UI_METERS child */

/* App launcher: tiles per row (remaining width split with UI_SEG_*). */
#define UI_LAUNCHER_COLS 5

/* UI_DIV fixed chrome rows (top, title, sep, sep, status, bottom) — for layout math. */
#define UI_DIV_CHROME       6
#define UI_DIV_INNER_W(c)   ((c) > 2 ? (c) - 2 : (c))
#define UI_LAUNCHER_AVAIL_H(r)  ((r) - 1 - UI_DIV_CHROME)   /* meter row + div chrome */
#define UI_GRID_MAX_H(r)    UI_LAUNCHER_AVAIL_H(r)           /* max grid rows on screen */
#define UI_GRID_NPAGES(n, cols, vrows) \
	(((n) + (cols) - 1) / (cols) + (vrows) - 1) / (vrows)
#define UI_GRID_PAGE_ROW(page, vrows)  ((page) * (vrows))
#define UI_GRID_PAGE_INDEX(sel, cols, vrows)  (((sel) / (cols)) / (vrows))

/* Relative layout: divide width into n equal segments (remainder distributed). */
#define UI_SEG_X(w, n, i)  (((w) * (i)) / (n))
#define UI_SEG_W(w, n, i)  (UI_SEG_X(w, n, (i) + 1) - UI_SEG_X(w, n, i))

/* Style flags (bit mask) */
#define UI_BOLD    0x01
#define UI_DIM     0x02
#define UI_INVERSE 0x04

/* Alignment */
#define UI_LEFT    0
#define UI_CENTER  1
#define UI_RIGHT   2

typedef struct {
    unsigned char fg;     /* ANSI color code (30-37 / 90-97); 0 = default */
    unsigned char bg;     /* ANSI color code (40-47);          0 = default */
    unsigned char flags;  /* UI_BOLD | UI_DIM | UI_INVERSE                 */
    unsigned char align;  /* UI_LEFT | UI_CENTER | UI_RIGHT                 */
} UiStyle;

typedef struct UiElem UiElem;
struct UiElem {
    UiElemType  type;
    UiStyle     style;
    int         h;      /* explicit height (rows); 0 = auto-sized from content */

    /* ── UI_SCREEN / UI_DIV: container fields ─────────────────────────── */
    const char *title;       /* DIV: title shown in the inverted bar         */
    const char *status;      /* DIV: bottom status-bar text                  */
    UiElem     *children;    /* child element array (stack / static)         */
    int         n_children;

    /* ── UI_HBAR: meter bar fields ─────────────────────────────────────── */
    const char *label;   /* left label, e.g. "CPU0"                          */
    const char *right;   /* right-aligned text inside bar, e.g. "75%"        */
    int         value;   /* numerator                                         */
    int         max;     /* denominator; 0 → empty bar                       */

    /* ── UI_TEXT: text line ────────────────────────────────────────────── */
    const char *text;

    /* ── UI_CANVAS: APU-rendered character grid ────────────────────────── */
    const char (*cells)[UI_CANVAS_WMAX]; /* pointer to caller's cell array  */
    const unsigned char (*cv_fg)[UI_CANVAS_WMAX];   /* optional per-cell fg */
    const unsigned char (*cv_g3)[UI_CANVAS_WMAX][3]; /* optional UTF-8 glyphs */
    int         cw;      /* active canvas width  (columns)                   */
    int         ch;      /* active canvas height (rows)                      */

    /* ── UI_GRID / UI_TILE: app launcher ───────────────────────────────── */
    int         n_cols;  /* GRID: tiles per row (>=1)                        */
    int         sel;     /* GRID: index of the highlighted child tile        */
    int         page;    /* GRID: 0-based page index (not a row offset)       */
    /* GRID reuses .ch as viewport rows per page; CANVAS uses .ch as height.  */
    /* TILE reuses .title (app name) and .text (icon line); selection
     * highlight is applied by the GRID renderer, not stored on the tile.   */
};

/* ── UI_HBAR helpers (ui_elem.c) ─────────────────────────────────────────── */

#define UI_HBAR_TENTHS_MAX  1000   /* 0–1000 → 0.0%–100.0% with one decimal  */

/* CPU meter colour thresholds (tenths, same scale as ui_hbar_init_pct). */
#define UI_CPU_LOAD_YELLOW  650    /* 65.0% — high load                       */
#define UI_CPU_LOAD_RED     850    /* 85.0% — very high, near 100%            */

extern const UiStyle ui_style_cpu_meter;

int  ui_utoa(unsigned v, char *out, int out_sz);
void ui_format_pct(unsigned tenths, char *out, int out_sz);
void ui_style_cpu_load(unsigned tenths, UiStyle *out);  /* green / yellow / red */
void ui_hbar_init(UiElem *e, const char *label, const char *right,
                  int value, int max, const UiStyle *style);
void ui_hbar_init_pct(UiElem *e, char *right_buf, int right_sz,
                      const char *label, unsigned tenths, const UiStyle *style);

#endif /* UI_ELEM_H */

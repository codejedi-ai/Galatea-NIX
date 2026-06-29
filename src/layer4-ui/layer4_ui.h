#ifndef LAYER4_UI_H
#define LAYER4_UI_H

#include "config.h"    /* UI_CANVAS_WMAX/HMAX/HMIN/CHROME (configurable canvas caps) */
#include "ui_elem.h"   /* UiElem declarative UI type system                          */

/*
 * Layer 4 UI TUI primitives.
 *
 * Authentic passive-matrix LCD look: a muted olive-gray screen tint with deep
 * charcoal "ink", an inverse-video Layer 4 UI title bar, bracketed stylus buttons,
 * thick horizontal rules, and a fixed ~40x22 handheld frame pinned top-left.
 */

/* Passive-LCD palette (256-color). */
#define UI_BG       "\033[48;5;187m"   /* soft olive-gray screen */
#define UI_FG       "\033[38;5;234m"   /* charcoal ink           */
#define UI_INV_ON   "\033[7m"          /* "pressed" stylus button */
#define UI_INV_OFF  "\033[27m"
#define UI_RESET    "\033[0m"
#define UI_HOME     "\033[H"
#define UI_GAME_BARS 1      /* one horizontal row at top for CPU0-3 meters */
#define UI_CLEAR    "\033[2J\033[H"
#define UI_HIDE_CUR "\033[?25l"
#define UI_SHOW_CUR "\033[?25h"
/* Synchronized output (DEC mode 2026): the terminal buffers everything between
 * BEGIN and END and paints it in one shot, so full-screen redraws don't tear or
 * jitter. Harmlessly ignored by terminals that don't support it. */
#define UI_SYNC_BEGIN "\033[?2026h"
#define UI_SYNC_END   "\033[?2026l"

/* Special key codes returned by ui_trygetch() for arrow keys (> 0xFF). */
#define UI_KEY_UP    0x101
#define UI_KEY_DOWN  0x102
#define UI_KEY_LEFT  0x103
#define UI_KEY_RIGHT 0x104

#define UI_W 38                        /* inner content width (cols inside │ │) */

void ui_begin(void);                   /* display_clear + hide cursor (app entry) */
void ui_end(void);                     /* display_clear before returning to launcher */
void ui_render(unsigned uptime_ticks); /* redraw the AI-match frame from g_rps */

/* Layer 4 UI launcher (home screen) with the app grid. */
void ui_home_render(unsigned uptime_ticks);

/* Interactive "You vs CPU" Rock-Paper-Scissors screen. moves are RPS_ROCK/.. or
 * -1 (none yet); last_result is RPS_R_* or 0. */
void ui_rps_human_render(unsigned uptime_ticks, int you, int cpu, int tie,
			   int your_move, int cpu_move, int last_result);
void ui_rps_human_reset(void);       /* call after ui_begin() before first render */

/* Blocking single-key read, paced by the clock so the CPU idles while waiting. */
unsigned char ui_getch(void);
int  ui_trygetch(void);                /* non-blocking: returns -1 if no key */
int  ui_getkey(void);                  /* blocking, decodes arrows (UI_KEY_*) */
int  ui_replay_prompt(int row, int col, int width); /* game-over: 'r' replay / 'q' quit */
void ui_getline(char *buf, int max);   /* clock-paced line input with echo */
unsigned ui_rng(void);                 /* small PRNG, mixed with the clock */

/* Character canvas for Layer 5 games, drawn inside the Layer 4 UI frame. Each
 * app owns its own UiCanvas instance (no shared global state) so two games can
 * never clobber each other's cells. Both width AND height are RUNTIME-adjustable
 * (autofit to the terminal, the `screen` command, and +/-); the maxima below are
 * the configurable storage caps (UI_CANVAS_WMAX/HMAX, defined in config.h). A
 * game fills cv->cells[][] then calls ui_canvas_draw(cv, ...). Always plot within
 * [0, ui_canvas_w(cv)) x [0, ui_canvas_h(cv)). */
typedef struct {
	int  w;                                     /* current active width  (20..WMAX) */
	int  h;                                     /* current active height (HMIN..HMAX) */
	char cells[UI_CANVAS_HMAX][UI_CANVAS_WMAX]; /* per-instance backing store */
	unsigned char fg[UI_CANVAS_HMAX][UI_CANVAS_WMAX];   /* ANSI fg 31-37/91-97; 0=default */
	unsigned char g3[UI_CANVAS_HMAX][UI_CANVAS_WMAX][3]; /* UTF-8 glyph; g3[][0]==0 → cells[][] */
} UiCanvas;

void ui_canvas_init(UiCanvas *cv);              /* default size + clear */
int  ui_canvas_w(const UiCanvas *cv);           /* current active width */
int  ui_canvas_h(const UiCanvas *cv);           /* current active height */
void ui_canvas_resize(UiCanvas *cv, int delta); /* widen/narrow by delta (clamped) */
void ui_canvas_autofit(UiCanvas *cv);           /* size to the whole terminal (ESC[6n) */
void ui_canvas_clear(UiCanvas *cv);
void ui_canvas_put_cell(UiCanvas *cv, int row, int col, char ch);
/* UTF-8 glyph (up to 3 bytes) with optional ANSI fg (31-37 / 91-97; 0 = default). */
void ui_canvas_put_glyph(UiCanvas *cv, int row, int col,
                         unsigned char b0, unsigned char b1, unsigned char b2,
                         unsigned char fg);
void ui_canvas_emit_cell(const UiCanvas *cv, int row, int col, char ch);
void ui_canvas_draw(const UiCanvas *cv, const char *title, unsigned uptime, const char *status);
void ui_canvas_draw_status(const UiCanvas *cv, const char *status);
void ui_canvas_draw_uptime(const UiCanvas *cv, unsigned uptime);
void ui_draw_stats_line(int row, int col);
/* Legacy no-op — UI_SCREEN auto-prepends UI_METERS in the display server. */
void ui_draw_game_bars(void);
void ui_draw_game_bars_invalidate(void);

/* universal screen size (the terminal is the screen) */
void screen_query(void);            /* refresh the cached terminal size */
void screen_query_terminal(void);   /* ESC[6n physical terminal (not display buf) */
int  screen_rows(void);             /* terminal rows (display server; cached) */
int  screen_cols(void);             /* terminal cols (display server; cached) */
void screen_set(int cols, int rows);/* fix the resolution via display server */
void screen_auto(void);             /* re-enable autofit (display server CPR) */
int  screen_is_manual(void);        /* 1 if display server has a manual size */
int  ui_term_cols(void);            /* legacy: columns only, re-queried */

#endif

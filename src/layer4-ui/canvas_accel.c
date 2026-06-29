#include "canvas_accel.h"
#include "../layer3-services/apu_client.h"
#include "screenbuf.h"

typedef struct {
    char (*cells)[UI_CANVAS_WMAX];
    int row_start;
    int row_end;
} BandArgs;

static BandArgs g_band_args[3];

static void band_clear_fn(void *arg)
{
    BandArgs *a = arg;
    for (int r = a->row_start; r < a->row_end; r++) {
        char *row = a->cells[r];
        for (int c = 0; c < UI_CANVAS_WMAX; c++)
            row[c] = ' ';
    }
}

/*
 * Clear all UI_CANVAS_HMAX rows in parallel across four cores.
 *
 * Layout for UI_CANVAS_HMAX = 60 (band = 15):
 *   Core 1: rows  0-14   (dispatched, WFE worker)
 *   Core 2: rows 15-29   (dispatched, WFE worker)
 *   Core 3: rows 30-44   (dispatched, WFE worker)
 *   Core 0: rows 45-59   (inline, while others run)
 *
 * Works for any HMAX: the last band absorbs the remainder so nothing is missed.
 * g_band_args is static; only one canvas clear may be in flight at a time,
 * which is guaranteed by the cooperative single-threaded scheduler on core 0.
 */
void ui_canvas_clear_parallel(UiCanvas *cv)
{
    int total = UI_CANVAS_HMAX;
    int band  = total / 4;

    ApuJob jobs[3];
    for (int i = 0; i < 3; i++) {
        g_band_args[i].cells     = cv->cells;
        g_band_args[i].row_start = i * band;
        g_band_args[i].row_end   = (i + 1) * band;
        jobs[i].fn  = band_clear_fn;
        jobs[i].arg = &g_band_args[i];
    }

    /* APUBatch dispatches all 3 in parallel and waits for them.
     * Core 0 clears the tail band (absorbing any HMAX % 4 remainder)
     * after the call returns. */
    APUBatch(jobs, 3);
    for (int r = 3 * band; r < total; r++) {
        char *row = cv->cells[r];
        for (int c = 0; c < UI_CANVAS_WMAX; c++)
            row[c] = ' ';
    }
}

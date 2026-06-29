#ifndef CANVAS_ACCEL_H
#define CANVAS_ACCEL_H

#include "layer4_ui.h"

/*
 * Parallel canvas operations backed by the accelerator cores (accel.h).
 *
 * ui_canvas_clear_parallel() splits the full UiCanvas backing store
 * (UI_CANVAS_HMAX × UI_CANVAS_WMAX) into 4 equal horizontal bands and
 * clears them simultaneously: cores 1-3 each take one band while core 0
 * clears the fourth band inline.  accel_wait_all() blocks until all bands
 * are done, so the function returns with the canvas fully cleared.
 *
 * Callers that previously called ui_canvas_clear() can switch to this
 * variant at no API cost: same signature, same post-condition.
 *
 * Requires accel_init() to have been called in kmain before any game task
 * runs (guaranteed by the accelerator bring-up in main.c).
 */
void ui_canvas_clear_parallel(UiCanvas *cv);

#endif

#ifndef MVC_H
#define MVC_H

/*
 * Textbook Model–View–Controller for Layer 5 games.
 *
 *   Model      — game state and rules only (scores, positions, physics).
 *                No terminal I/O, no escape sequences, no DisplayServer calls.
 *
 *   View       — presentation only. Reads the Model (const) and writes pixels/
 *                cells through Layer 4 (canvas / DisplayServer). No game rules.
 *
 *   Controller — glue between input, Model, and View. Maps keys to Model
 *                updates, runs Model::tick(), then View::render(). Implements
 *                the Game callbacks consumed by game_run().
 *
 * Typical layout per app:
 *
 *   foo_model.h / foo_model.c
 *   foo_view.h  / foo_view.c
 *   foo_controller.h / foo_controller.c
 *   foo.c       — app entry: static Controller + Game vtable → game_run()
 *
 * game_run() is the outer application shell (quit, pause, replay); it is not
 * part of MVC — it drives the Controller each frame.
 */

#endif

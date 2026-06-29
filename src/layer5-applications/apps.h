#ifndef APPS_H
#define APPS_H

/*
 * Layer 5 — User Applications.
 *
 * Unprivileged apps launched from the Layer 4 UI launcher (Layer 4). Each takes
 * over the terminal, runs its own loop, and returns to the launcher when the
 * user quits. They build on Layer 4 services (clock, RPS server) via IPC and on
 * the Layer 4 UI canvas for drawing.
 */
void app_rps(void);      /* Rock-Paper-Scissors vs the computer */
void app_snake(void);    /* classic snake */
void app_pong(void);     /* pong vs a simple AI paddle */
void app_tetris(void);   /* classic falling blocks */

/* small libc-free string helpers shared by the apps */
int  app_uitoa(unsigned v, char *out);             /* write decimal; return length */
void app_append(char *dst, int *pos, const char *src);  /* append src at *pos */

#endif

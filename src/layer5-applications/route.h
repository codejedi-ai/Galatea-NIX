#ifndef ROUTE_H
#define ROUTE_H

#include "track.h"

#define ROUTE_MAX_PATH  256   /* max nodes in a route */

/* A single step in a route at a branch node */
typedef struct {
    track_node *branch;   /* the branch node */
    int         dir;      /* DIR_STRAIGHT or DIR_CURVED */
} SwitchAction;

typedef struct {
    track_node    *nodes[ROUTE_MAX_PATH];   /* ordered nodes in the path  */
    int            len;                     /* number of nodes             */
    SwitchAction   sw[ROUTE_MAX_PATH / 2];  /* switches to set             */
    int            sw_count;
    int            total_dist;             /* mm, start-to-end            */
} Route;

/*
 * route_find — BFS from `from` to `to`, ignoring switch direction.
 * Returns 0 on success, -1 if no path found.
 * `from` and `to` must be NODE_SENSOR nodes.
 */
int route_find(track_node *from, track_node *to, Route *out);

/*
 * route_apply_switches — send all switch commands for `r` to the Marklin server.
 */
void route_apply_switches(const Route *r);

/*
 * route_dist_remaining — distance in mm from sensor `at` to route destination.
 * Returns -1 if `at` is not on the route.
 */
int route_dist_remaining(const Route *r, track_node *at);

/*
 * route_next_sensor — the next sensor node after `at` on the route.
 * Returns NULL if `at` is the last sensor or not found.
 */
track_node *route_next_sensor(const Route *r, track_node *at);

/*
 * track_find_sensor — look up a sensor node by name (e.g. "A3", "B12").
 */
track_node *track_find_sensor(track_node *track, const char *name);

#endif /* ROUTE_H */

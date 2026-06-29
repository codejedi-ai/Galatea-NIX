#ifndef TRACK_H
#define TRACK_H

/*
 * Track graph node/edge types for the Marklin track layout.
 * Ported from the CS452 course track data format.
 * Freestanding (no stdlib dependencies).
 */

#define TRACK_MAX 144   /* nodes in track A or B */

typedef enum {
    NODE_NONE,
    NODE_SENSOR,
    NODE_BRANCH,
    NODE_MERGE,
    NODE_ENTER,
    NODE_EXIT,
} node_type;

#define DIR_AHEAD    0
#define DIR_STRAIGHT 0
#define DIR_CURVED   1

struct track_node;
typedef struct track_node track_node;
typedef struct track_edge  track_edge;

struct track_edge {
    track_edge *reverse;
    track_node *src, *dest;
    int dist;           /* millimetres */
};

struct track_node {
    const char *name;
    node_type   type;
    int         num;    /* sensor index (0-79) or switch number */
    track_node *reverse;
    track_edge  edge[2];
};

void init_tracka(track_node *track);
void init_trackb(track_node *track);

#endif /* TRACK_H */

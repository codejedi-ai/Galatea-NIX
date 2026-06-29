#include "route.h"
#include "../layer3-services/marklin.h"

/* ---- tiny string helpers (no libc) --------------------------------------- */

static int r_strlen(const char *s) { int n = 0; while (s[n]) n++; return n; }

static int r_streq(const char *a, const char *b)
{
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == 0 && *b == 0;
}

/* ---- BFS queue ----------------------------------------------------------- */

#define BFS_SIZE TRACK_MAX

typedef struct {
    int node_idx;   /* index into track[] */
    int parent;     /* parent index in bfs_q, -1 = root */
    int dir;        /* which edge was followed from parent's node */
} BFSNode;

/* ---- route_find ---------------------------------------------------------- */

int route_find(track_node *from, track_node *to, Route *out)
{
    static BFSNode bfs_q[BFS_SIZE];
    static int     visited[TRACK_MAX];

    int head = 0, tail = 0;

    /* Clear visited map.  We identify nodes by pointer offset from &track[0].
     * Since we don't have the track base here, we use the node pointer itself
     * and store it in visited[].  Use a linear scan (TRACK_MAX = 144, fast). */
    for (int i = 0; i < TRACK_MAX; i++) visited[i] = 0;

    /* Determine track base from from's own structure.
     * We walk back: from->reverse->reverse == from (sensor nodes).
     * We can't get the base directly, so we use pointer arithmetic with the
     * fact that nodes are laid out in a contiguous array.
     * Instead, mark visited using a bitset indexed by (node - from) / sizeof. */

    /* Simpler: use a flat visited array indexed by node pointer.
     * Map node → index by checking (node - from) % sizeof(track_node).
     * from is node 0 of the sensor block; but branches can be at higher indices.
     * We'll use the node's `num` field to distinguish sensors (0-79).
     * For non-sensor nodes we need another approach.
     *
     * Best approach: pass the track base to route_find.
     * But to keep the API simple we derive it: from is always a sensor (num 0-79),
     * so track base = from - from->num  (pointer arithmetic). */

    track_node *base = from - from->num;
    /* Sanity: if from->num >= TRACK_MAX, something is wrong */
    if ((int)(from - base) < 0 || (int)(from - base) >= TRACK_MAX) return -1;

    bfs_q[tail++] = (BFSNode){ (int)(from - base), -1, 0 };
    visited[(int)(from - base)] = 1;

    int found = -1;
    while (head < tail && found < 0) {
        BFSNode cur = bfs_q[head++];
        track_node *n = base + cur.node_idx;

        /* Expand neighbours */
        int max_dir = (n->type == NODE_BRANCH) ? 2 : 1;
        for (int d = 0; d < max_dir; d++) {
            track_node *next = n->edge[d].dest;
            if (!next) continue;
            int ni = (int)(next - base);
            if (ni < 0 || ni >= TRACK_MAX) continue;
            if (visited[ni]) continue;
            visited[ni] = 1;
            if (tail >= BFS_SIZE) break;
            bfs_q[tail++] = (BFSNode){ ni, head - 1, d };
            if (next == to) { found = tail - 1; break; }
        }
    }

    if (found < 0) return -1;   /* no path */

    /* Reconstruct path (reverse order) */
    int rev[ROUTE_MAX_PATH], rlen = 0;
    int cur = found;
    while (cur >= 0 && rlen < ROUTE_MAX_PATH) {
        rev[rlen++] = cur;
        cur = bfs_q[cur].parent;
    }

    /* Fill out Route (forward order) */
    out->len        = 0;
    out->sw_count   = 0;
    out->total_dist = 0;

    for (int i = rlen - 1; i >= 0; i--) {
        BFSNode *b = &bfs_q[rev[i]];
        track_node *n = base + b->node_idx;
        out->nodes[out->len++] = n;

        /* Record switch action if this node is a branch */
        if (n->type == NODE_BRANCH && i > 0) {
            /* The direction taken from this branch is stored in the child */
            BFSNode *child = &bfs_q[rev[i - 1]];
            out->sw[out->sw_count].branch = n;
            out->sw[out->sw_count].dir    = child->dir;
            out->sw_count++;
        }
    }

    /* Compute total distance along path edges */
    for (int i = 0; i < out->len - 1; i++) {
        track_node *n = out->nodes[i];
        track_node *nx = out->nodes[i + 1];
        /* find which edge leads to nx */
        if (n->edge[0].dest == nx) out->total_dist += n->edge[0].dist;
        else if (n->type == NODE_BRANCH && n->edge[1].dest == nx)
            out->total_dist += n->edge[1].dist;
    }

    return 0;
}

/* ---- route_apply_switches ------------------------------------------------ */

void route_apply_switches(const Route *r)
{
    for (int i = 0; i < r->sw_count; i++) {
        int sw_num = r->sw[i].branch->num;
        int dir = (r->sw[i].dir == DIR_CURVED)
                  ? MARKLIN_SW_CURVED : MARKLIN_SW_STRAIGHT;
        MarklinkSetSwitch(sw_num, dir);
    }
}

/* ---- route_dist_remaining ------------------------------------------------ */

int route_dist_remaining(const Route *r, track_node *at)
{
    int dist = 0;
    int found = 0;
    for (int i = r->len - 1; i >= 0; i--) {
        if (r->nodes[i] == at) { found = 1; break; }
        track_node *n  = r->nodes[i];
        track_node *np = (i > 0) ? r->nodes[i - 1] : 0;
        if (np) {
            if (np->edge[0].dest == n) dist += np->edge[0].dist;
            else if (np->type == NODE_BRANCH && np->edge[1].dest == n)
                dist += np->edge[1].dist;
        }
    }
    return found ? dist : -1;
}

/* ---- route_next_sensor --------------------------------------------------- */

track_node *route_next_sensor(const Route *r, track_node *at)
{
    int i;
    /* find `at` in route */
    for (i = 0; i < r->len; i++) {
        if (r->nodes[i] == at) break;
    }
    if (i >= r->len) return 0;
    /* scan forward for next sensor */
    for (i++; i < r->len; i++) {
        if (r->nodes[i]->type == NODE_SENSOR) return r->nodes[i];
    }
    return 0;
}

/* ---- track_find_sensor --------------------------------------------------- */

track_node *track_find_sensor(track_node *track, const char *name)
{
    int len = r_strlen(name);
    for (int i = 0; i < 80; i++) {   /* sensors are first 80 nodes */
        if (r_streq(track[i].name, name)) return &track[i];
    }
    (void)len;
    return 0;
}

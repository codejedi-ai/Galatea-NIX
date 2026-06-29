/*
 * tc1.c — Train Controller 1 (CS452/652 Spring 2026, Milestone 1)
 *
 * Capability: find where train 1 is, run it in a calibration loop,
 * then stop it precisely at a user-specified sensor location.
 *
 * Design:
 *   tc1_entry()        — server task.  Receives sensor events, display ticks,
 *                        and shell commands.  Never blocks on hardware directly.
 *   tc1_sensor_notif() — notifier: polls Marklin sensors every 100 ms, sends
 *                        TC1_MSG_SENSOR to TC1 server.
 *   tc1_display_notif()— notifier: sends TC1_MSG_TICK every 1 s (10 Hz clock).
 *
 * Position model:
 *   last_node   = most recently triggered sensor (track_node*)
 *   last_time   = clock tick when last_node triggered
 *   velocity    = last edge distance / (last_time delta)   [mm per tick]
 *   pred_time   = last_time + next_edge_dist / velocity
 *
 * Stopping:
 *   When route is active and distance remaining <= stop_dist[speed],
 *   speed is set to 0.  stop_dist is calibrated offline (see tc1.h).
 *
 * Prediction display (assignment requirement):
 *   Each sensor hit prints: predicted time, actual time, error in ms and mm.
 */

#include "tc1.h"
#include "track.h"
#include "route.h"
#include "../layer1-processes/syscall.h"
#include "../layer1-processes/rpi.h"
#include "../layer3-services/clockserver.h"
#include "../layer3-services/clock_client.h"
#include "../layer3-services/nameserver.h"
#include "../layer3-services/marklin.h"

/* ---- calibration tables -------------------------------------------------- */

static int vel_table[15]  = TC1_VEL_TABLE;   /* mm/s at speed 0-14          */
static int stop_table[15] = TC1_STOP_TABLE;  /* stopping distance mm         */

/* ---- global tid ---------------------------------------------------------- */

static volatile int tc1_tid_g = -1;
int TC1Tid(void) { return tc1_tid_g; }

/* ---- tiny helpers -------------------------------------------------------- */

static int t_strlen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int t_abs(int x) { return x < 0 ? -x : x; }

/* int-to-decimal into buf, returns length */
static int i2d(int v, char *buf)
{
    if (v == 0) { buf[0] = '0'; buf[1] = 0; return 1; }
    char tmp[12]; int n = 0, neg = 0, p;
    if (v < 0) { neg = 1; v = -v; }
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    if (neg) tmp[n++] = '-';
    for (p = 0; p < n; p++) buf[p] = tmp[n - 1 - p];
    buf[n] = 0;
    return n;
}

/* ---- OSC 452 state frame ------------------------------------------------- */
/*
 * ESC ] 452 ; {"t":<addr>,"v":<vel>,"s":"<Name>","i":<idx>,"ts":<ticks>} BEL
 * xterm.js ignores it; screen_bridge.py extracts it for /state-ws.
 */
static void emit_state(int addr, int vel_mmps, const char *sens_name,
                        int sens_idx, int ts)
{
    char num[16];
    uart_putc(CONSOLE, '\x1b'); uart_putc(CONSOLE, ']');
    uart_putc(CONSOLE, '4');    uart_putc(CONSOLE, '5'); uart_putc(CONSOLE, '2');
    uart_putc(CONSOLE, ';');
    uart_putc(CONSOLE, '{');
    /* "t": */
    uart_putc(CONSOLE, '"'); uart_putc(CONSOLE, 't'); uart_putc(CONSOLE, '"');
    uart_putc(CONSOLE, ':');
    i2d(addr, num); for (int i = 0; num[i]; i++) uart_putc(CONSOLE, num[i]);
    /* ,"v": */
    uart_putc(CONSOLE, ',');
    uart_putc(CONSOLE, '"'); uart_putc(CONSOLE, 'v'); uart_putc(CONSOLE, '"');
    uart_putc(CONSOLE, ':');
    i2d(vel_mmps, num); for (int i = 0; num[i]; i++) uart_putc(CONSOLE, num[i]);
    /* ,"s":"<Name>" */
    uart_putc(CONSOLE, ',');
    uart_putc(CONSOLE, '"'); uart_putc(CONSOLE, 's'); uart_putc(CONSOLE, '"');
    uart_putc(CONSOLE, ':');
    uart_putc(CONSOLE, '"');
    for (int i = 0; sens_name[i]; i++) uart_putc(CONSOLE, sens_name[i]);
    uart_putc(CONSOLE, '"');
    /* ,"i": */
    uart_putc(CONSOLE, ',');
    uart_putc(CONSOLE, '"'); uart_putc(CONSOLE, 'i'); uart_putc(CONSOLE, '"');
    uart_putc(CONSOLE, ':');
    i2d(sens_idx, num); for (int i = 0; num[i]; i++) uart_putc(CONSOLE, num[i]);
    /* ,"ts": */
    uart_putc(CONSOLE, ',');
    uart_putc(CONSOLE, '"'); uart_putc(CONSOLE, 't'); uart_putc(CONSOLE, 's');
    uart_putc(CONSOLE, '"'); uart_putc(CONSOLE, ':');
    i2d(ts, num); for (int i = 0; num[i]; i++) uart_putc(CONSOLE, num[i]);
    uart_putc(CONSOLE, '}');
    uart_putc(CONSOLE, '\x07');
}

/* ---- idle time (read from idle.c globals) -------------------------------- */

extern volatile unsigned long long g_idle_us;
extern volatile unsigned long long g_total_us;

/* ---- track graph + state ------------------------------------------------- */

#define TC1_STATE_DISCOVER 0   /* waiting for first sensor hit */
#define TC1_STATE_RUNNING  1   /* train running, tracking sensors */
#define TC1_STATE_ROUTING  2   /* goto active, switches set */
#define TC1_STATE_STOPPED  3   /* train stopped at destination */

typedef struct {
    /* Train */
    int         addr;
    int         speed;
    int         state;

    /* Position */
    track_node *last_node;      /* last triggered sensor node */
    int         last_time;      /* tick when last_node triggered */

    /* Velocity model (updated on each sensor-to-sensor crossing) */
    int         velocity;       /* mm per tick (× 100 for precision → stored as mm/s) */

    /* Prediction (for error display) */
    track_node *pred_next;      /* predicted next sensor */
    int         pred_time;      /* predicted tick for pred_next */
    int         pred_dist;      /* mm from last_node to pred_next */

    /* Route */
    Route       route;
    track_node *dest;           /* destination sensor node, or NULL */

    /* Track graph */
    track_node  track[TRACK_MAX];

    /* Calibration update */
    unsigned long long idle_snap;  /* snapshot of g_idle_us at last display tick */
    unsigned long long total_snap; /* snapshot of g_total_us at last display tick */
} TC1State;

/* ---- velocity helper ----------------------------------------------------- */

static int vel_mmps(int speed)
{
    if (speed < 0) speed = 0;
    if (speed > 14) speed = 14;
    return vel_table[speed];
}

static int stop_mm(int speed)
{
    if (speed < 0) speed = 0;
    if (speed > 14) speed = 14;
    return stop_table[speed];
}

/* ---- distance to next sensor on route ------------------------------------ */
/*
 * Given current position `at` and the active route `r`, compute the mm
 * distance to the next sensor node.  Returns 0 if unknown.
 */
static int dist_to_next_sensor(const Route *r, track_node *at)
{
    int dist = 0;
    int found = 0;
    for (int i = 0; i < r->len - 1; i++) {
        track_node *n  = r->nodes[i];
        track_node *nx = r->nodes[i + 1];
        /* accumulate edge dist */
        int edge_dist = 0;
        if (n->edge[0].dest == nx) edge_dist = n->edge[0].dist;
        else if (n->type == NODE_BRANCH && n->edge[1].dest == nx)
            edge_dist = n->edge[1].dist;
        if (!found && n == at) { found = 1; dist = 0; }
        if (found) {
            dist += edge_dist;
            if (nx->type == NODE_SENSOR) return dist;
        }
    }
    /* Not on this route — fall back to direct edge */
    if (!found && at && at->edge[0].dest) {
        track_node *nx = at->edge[0].dest;
        return at->edge[0].dist + (nx->type == NODE_SENSOR ? 0 : nx->edge[0].dist);
    }
    return 0;
}

/* ---- sensor event processing --------------------------------------------- */

static void process_sensor(TC1State *s, track_node *hit, int now)
{
    /* ---- prediction error display ---- */
    if (s->pred_next && s->pred_next == hit && s->pred_time > 0) {
        int err_ticks = now - s->pred_time;   /* + = late, - = early */
        /* velocity in mm per tick: vel_mmps(speed) / 100 */
        /* dist_err = velocity * err_ticks  but velocity stored as mm/s */
        /* 1 tick = 10 ms = 0.01 s, so dist_err = vel * err_ticks * 10 / 1000 mm */
        int v = s->velocity;  /* mm/s */
        int dist_err_mm = v * err_ticks / 100; /* mm (positive = overshot) */
        int err_ms = err_ticks * 10;

        uart_printf(CONSOLE,
            "\r\n\033[33m[TC1] pred %-4s  exp %d ms  got %d ms  "
            "err %c%d ms  (%c%d mm)\033[0m\r\n",
            hit->name,
            s->pred_dist * 1000 / (v ? v : 1),
            s->pred_dist * 1000 / (v ? v : 1) + err_ms,
            (err_ms < 0 ? '-' : '+'), t_abs(err_ms),
            (dist_err_mm < 0 ? '-' : '+'), t_abs(dist_err_mm));
    }

    /* ---- velocity update ---- */
    if (s->last_node && s->last_time > 0) {
        int dt_ticks = now - s->last_time;
        /* Compute edge distance from last_node to hit along the route/track */
        int dist_mm = 0;
        if (s->state == TC1_STATE_ROUTING || s->state == TC1_STATE_RUNNING) {
            /* Use route distance if available */
            for (int i = 0; i < s->route.len - 1; i++) {
                if (s->route.nodes[i] == s->last_node &&
                    s->route.nodes[i + 1] == hit) {
                    dist_mm = s->route.nodes[i]->edge[0].dist;
                    break;
                }
                /* Also check segment sums if there are non-sensor nodes between */
                if (s->route.nodes[i] == s->last_node) {
                    /* Sum until we hit `hit` */
                    for (int j = i; j < s->route.len - 1; j++) {
                        track_node *n  = s->route.nodes[j];
                        track_node *nx = s->route.nodes[j + 1];
                        int ed = 0;
                        if (n->edge[0].dest == nx) ed = n->edge[0].dist;
                        else if (n->type == NODE_BRANCH && n->edge[1].dest == nx)
                            ed = n->edge[1].dist;
                        dist_mm += ed;
                        if (nx == hit) break;
                    }
                    break;
                }
            }
            /* Fallback: direct edge distance */
            if (dist_mm == 0 && s->last_node->edge[0].dest == hit)
                dist_mm = s->last_node->edge[0].dist;
        }

        if (dt_ticks > 0 && dist_mm > 0) {
            /* velocity mm/s = dist_mm * 100 / dt_ticks  (1 tick = 10 ms) */
            int new_vel = dist_mm * 100 / dt_ticks;
            /* EMA: smooth 80% old + 20% new */
            s->velocity = (s->velocity * 4 + new_vel) / 5;

            /* update calibration table slot for current speed */
            if (s->speed >= 1 && s->speed <= 14)
                vel_table[s->speed] = (vel_table[s->speed] * 4 + new_vel) / 5;
        }
    }

    /* ---- update position ---- */
    s->last_node = hit;
    s->last_time = now;

    if (s->state == TC1_STATE_DISCOVER) {
        s->state = TC1_STATE_RUNNING;
        uart_printf(CONSOLE, "\r\n\033[32m[TC1] Train found @ %s\033[0m\r\n",
                    hit->name);
    }

    /* ---- compute prediction for next sensor ---- */
    s->pred_next = 0;
    s->pred_dist = 0;
    s->pred_time = 0;
    if (s->state == TC1_STATE_ROUTING) {
        s->pred_next = route_next_sensor(&s->route, hit);
        if (s->pred_next) {
            s->pred_dist = dist_to_next_sensor(&s->route, hit);
            int v = s->velocity > 0 ? s->velocity : vel_mmps(s->speed);
            s->pred_time = now + (s->pred_dist * 100) / v;
        }
    } else if (s->state == TC1_STATE_RUNNING) {
        /* Free-running: predict along edge[0] */
        track_node *nx = hit->edge[0].dest;
        while (nx && nx->type != NODE_SENSOR) nx = nx->edge[0].dest;
        if (nx) {
            s->pred_next = nx;
            s->pred_dist = hit->edge[0].dist; /* rough: just one hop */
            int v = s->velocity > 0 ? s->velocity : vel_mmps(s->speed);
            s->pred_time = now + (s->pred_dist * 100) / v;
        }
    }

    /* ---- emit OSC 452 state frame ---- */
    emit_state(s->addr, s->velocity, hit->name, hit->num, now);

    /* ---- routing: check if we should stop ---- */
    if (s->state == TC1_STATE_ROUTING && s->dest) {
        int dist_rem = route_dist_remaining(&s->route, hit);
        int sdist    = stop_mm(s->speed);

        uart_printf(CONSOLE,
            "\r\n[TC1] @ %s  dist_to_dest=%d mm  stop_dist=%d mm\r\n",
            hit->name, dist_rem, sdist);

        if (dist_rem >= 0 && dist_rem <= sdist + 50) {
            /* Time to stop */
            MarklinkSetSpeed(s->addr, 0);
            s->speed  = 0;
            s->state  = TC1_STATE_STOPPED;
            uart_printf(CONSOLE,
                "\r\n\033[32m[TC1] Stop command sent  (dist_rem=%d, sdist=%d)\033[0m\r\n",
                dist_rem, sdist);
        }
    }
}

/* ---- handle goto command ------------------------------------------------- */

static void handle_goto(TC1State *s, const char *dest_name)
{
    if (s->state == TC1_STATE_DISCOVER) {
        uart_printf(CONSOLE, "[TC1] still discovering train position\r\n");
        return;
    }
    if (!s->last_node) {
        uart_printf(CONSOLE, "[TC1] no known position\r\n");
        return;
    }

    track_node *dest = track_find_sensor(s->track, dest_name);
    if (!dest) {
        uart_printf(CONSOLE, "[TC1] unknown sensor '%s'\r\n", dest_name);
        return;
    }

    /* BFS route from current sensor to dest */
    int rc = route_find(s->last_node, dest, &s->route);
    if (rc < 0) {
        uart_printf(CONSOLE, "[TC1] no route from %s to %s\r\n",
                    s->last_node->name, dest_name);
        return;
    }

    /* Apply switches */
    route_apply_switches(&s->route);

    s->dest  = dest;
    s->state = TC1_STATE_ROUTING;

    uart_printf(CONSOLE,
        "\r\n\033[36m[TC1] Routing %s → %s  dist=%d mm  %d switches\033[0m\r\n",
        s->last_node->name, dest_name, s->route.total_dist, s->route.sw_count);
}

/* ---- display status (called on TC1_MSG_TICK) ----------------------------- */

static void display_status(TC1State *s, int now)
{
    /* Idle % */
    unsigned long long idle_now  = g_idle_us;
    unsigned long long total_now = g_total_us;
    unsigned long long d_idle    = idle_now  - s->idle_snap;
    unsigned long long d_total   = total_now - s->total_snap;
    int idle_pct = (d_total > 0) ? (int)(d_idle * 100 / d_total) : 0;
    s->idle_snap  = idle_now;
    s->total_snap = total_now;

    const char *sn = (s->last_node) ? s->last_node->name : "---";
    const char *dn = (s->dest) ? s->dest->name : "---";

    uart_printf(CONSOLE,
        "\033[s\033[1;1H\033[K"   /* save, goto row 1, clear line */
        "\033[36m[TC1] t=%d  spd=%d  vel=%d mm/s  @%s  dst=%s  idle=%d%%\033[0m"
        "\033[u",
        now, s->speed, s->velocity, sn, dn, idle_pct);
}

/* =========================================================================
 * Notifier tasks
 * ========================================================================= */

void tc1_sensor_notif(void)
{
    int clock = ClockServerTid();
    int tc1   = TC1Tid();
    if (tc1 < 0) return;

    TC1Msg m;
    m.type = TC1_MSG_SENSOR;
    int ack = 0;

    for (;;) {
        Delay(clock, TC1_POLL_TICKS);
        m.sensors = MarklinkPollSensors();
        Send(tc1, (const char *)&m, (int)sizeof(m),
             (char *)&ack, (int)sizeof(ack));
    }
}

void tc1_display_notif(void)
{
    int clock = ClockServerTid();
    int tc1   = TC1Tid();
    if (tc1 < 0) return;

    TC1Msg m;
    m.type = TC1_MSG_TICK;
    m.speed = 0;
    int ack = 0;

    for (;;) {
        Delay(clock, 100);   /* 1 second */
        Send(tc1, (const char *)&m, (int)sizeof(m),
             (char *)&ack, (int)sizeof(ack));
    }
}

/* =========================================================================
 * TC1 main server task
 * ========================================================================= */

void tc1_entry(void)
{
    tc1_tid_g = MyTid();
    RegisterAs(TC1_SERVER_NAME);

    /* Spawn notifiers */
    Create(TC1_NOTIF_PRIORITY, tc1_sensor_notif);
    Create(TC1_NOTIF_PRIORITY, tc1_display_notif);

    /* Allocate state on the stack (TC1 is a long-lived task) */
    static TC1State s;
    s.addr      = TC1_TRAIN_ADDR;
    s.speed     = TC1_DEFAULT_SPEED;
    s.state     = TC1_STATE_DISCOVER;
    s.last_node = 0;
    s.last_time = 0;
    s.velocity  = vel_mmps(TC1_DEFAULT_SPEED);
    s.pred_next = 0;
    s.pred_time = 0;
    s.pred_dist = 0;
    s.dest      = 0;
    s.route.len = 0;
    s.idle_snap = 0;
    s.total_snap = 0;

    /* Initialize track graph (Track A) */
    init_tracka(s.track);

    int clock = ClockServerTid();

    /* Start the train */
    MarklinkSetSpeed(s.addr, s.speed);
    uart_printf(CONSOLE,
        "\r\n\033[32m[TC1] started — train %d speed %d — discovering...\033[0m\r\n",
        s.addr, s.speed);

    /* Main receive loop */
    int from;
    TC1Msg msg;
    int ack = 0;

    for (;;) {
        Receive(&from, (char *)&msg, (int)sizeof(msg));
        Reply(from, (const char *)&ack, (int)sizeof(ack));

        int now = Time(clock);

        switch (msg.type) {

        case TC1_MSG_SENSOR: {
            /* Scan all 80 sensors for newly active ones */
            const MarklinkSensors *snap = &msg.sensors;
            for (int mod = 1; mod <= MARKLIN_MODULES; mod++) {
                for (int sens = 1; sens <= MARKLIN_SENS_PER_M; sens++) {
                    if (!marklin_sens_get(snap, mod, sens)) continue;
                    /* Sensor (mod, sens) is active */
                    int idx = (mod - 1) * MARKLIN_SENS_PER_M + (sens - 1);
                    track_node *hit = &s.track[idx];
                    /* Debounce: skip if same sensor as last hit */
                    if (hit == s.last_node) continue;
                    process_sensor(&s, hit, now);
                    break;  /* process first active sensor per poll */
                }
            }
            break;
        }

        case TC1_MSG_TICK:
            display_status(&s, now);
            break;

        case TC1_MSG_GOTO:
            handle_goto(&s, msg.dest);
            break;

        case TC1_MSG_SPEED:
            s.speed = msg.speed;
            MarklinkSetSpeed(s.addr, s.speed);
            s.velocity = vel_mmps(s.speed);
            uart_printf(CONSOLE, "[TC1] speed set to %d\r\n", s.speed);
            break;

        case TC1_MSG_STOP:
            MarklinkSetSpeed(s.addr, 0);
            s.speed = 0;
            s.state = TC1_STATE_STOPPED;
            uart_printf(CONSOLE, "[TC1] stopped\r\n");
            break;
        }
    }
}

/* =========================================================================
 * Client API (called from shell task)
 * ========================================================================= */

int TC1Goto(const char *sensor_name)
{
    int tc1 = WhoIs(TC1_SERVER_NAME);
    if (tc1 < 0) return -1;

    TC1Msg m;
    m.type = TC1_MSG_GOTO;
    int n = t_strlen(sensor_name);
    if (n > 11) n = 11;
    for (int i = 0; i < n; i++) m.dest[i] = sensor_name[i];
    m.dest[n] = 0;

    int ack = 0;
    Send(tc1, (const char *)&m, (int)sizeof(m),
         (char *)&ack, (int)sizeof(ack));
    return ack;
}

int TC1Speed(int speed)
{
    int tc1 = WhoIs(TC1_SERVER_NAME);
    if (tc1 < 0) return -1;

    TC1Msg m;
    m.type  = TC1_MSG_SPEED;
    m.speed = speed;

    int ack = 0;
    Send(tc1, (const char *)&m, (int)sizeof(m),
         (char *)&ack, (int)sizeof(ack));
    return ack;
}

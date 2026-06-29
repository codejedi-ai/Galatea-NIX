#include "observer.h"
#include "../layer1-processes/syscall.h"  /* Send() for TidObserver */

/* ================================================================ Subject == */

void subject_init(Subject *s)
{
    s->count = 0;
}

int subject_attach(Subject *s, Observer *o)
{
    if (!o) return -1;
    /* ignore duplicate registration */
    for (int i = 0; i < s->count; i++)
        if (s->obs[i] == o) return 0;
    if (s->count >= SUBJECT_MAX_OBS) return -1;
    s->obs[s->count++] = o;
    return 0;
}

void subject_detach(Subject *s, Observer *o)
{
    for (int i = 0; i < s->count; i++) {
        if (s->obs[i] == o) {
            /* swap with last to keep the list compact */
            s->obs[i] = s->obs[--s->count];
            return;
        }
    }
}

void subject_notify(Subject *s, int event, int value)
{
    /* snapshot count: an update() must not attach/detach during iteration */
    int n = s->count;
    for (int i = 0; i < n; i++) {
        Observer *o = s->obs[i];
        if (o && o->update)
            o->update(o, event, value);
    }
}

/* ========================================================= FnObserver == */

static void fn_update(Observer *self, int event, int value)
{
    FnObserver *fo = (FnObserver *)self;
    if (fo->fn) fo->fn(fo->ctx, event, value);
}

void fn_observer_init(FnObserver *o,
                      void (*fn)(void *ctx, int event, int value),
                      void *ctx)
{
    o->base.update = fn_update;
    o->fn  = fn;
    o->ctx = ctx;
}

/* ======================================================= TidObserver == */

/*
 * Send a two-word ObsMsg to the target task and wait for a one-word reply.
 * The target must be blocked in Receive(); it calls Reply() to release us.
 *
 * Contract: callers of subject_notify() that have TidObserver subscribers
 * must ensure those targets are in Receive() before calling notify().
 * This matches the CS452 server / notifier contract exactly.
 */
static void tid_update(Observer *self, int event, int value)
{
    TidObserver *to = (TidObserver *)self;
    ObsMsg msg = { event, value };
    int reply;
    Send(to->tid, (const char *)&msg, (int)sizeof(msg),
         (char *)&reply, (int)sizeof(reply));
}

void tid_observer_init(TidObserver *o, int tid)
{
    o->base.update = tid_update;
    o->tid         = tid;
}

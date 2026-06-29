#ifndef OBSERVER_H
#define OBSERVER_H

/*
 * Textbook GoF Observer pattern — bare-metal C implementation.
 *
 * ┌─────────────────────────────────────────────────────────────────────┐
 * │  Subject                    Observer (interface / abstract base)    │
 * │  ──────────────────────     ─────────────────────────────────────── │
 * │  obs[SUBJECT_MAX_OBS]  ───▶ update(self, event, value)              │
 * │  count                                                              │
 * │  subject_attach(o)          Concrete observers implement update():  │
 * │  subject_detach(o)            • FnObserver  — synchronous callback  │
 * │  subject_notify(e,v) ──────▶  • TidObserver — IPC to another task  │
 * └─────────────────────────────────────────────────────────────────────┘
 *
 * C polymorphism trick: embed Observer as the FIRST member of any concrete
 * observer struct.  A (ConcreteObserver *) can then be safely cast to
 * (Observer *) and back, because the first-member address equals the
 * struct address in standard C (§6.7.2.1).
 *
 * Lifecycle
 * ---------
 * 1. Initialise a subject:            subject_init(&subj);
 * 2. Create a concrete observer:      fn_observer_init(&fobs, my_fn, ctx);
 * 3. Register it:                     subject_attach(&subj, &fobs.base);
 * 4. Fire notifications:              subject_notify(&subj, event, value);
 *    → my_fn(ctx, event, value) is called synchronously for each observer.
 * 5. Unregister:                      subject_detach(&subj, &fobs.base);
 *
 * Cross-task use (TidObserver)
 * ----------------------------
 * When a subject and an observer live in different kernel tasks, use
 * TidObserver.  Its update() calls Send({event,value}) to the target task.
 * The target MUST be blocked in Receive(); it must Reply() to unblock the
 * sender.  This matches the CS452 server pattern exactly:
 *
 *   subject fires → TidObserver.update() → Send(target_tid, ObsMsg)
 *   target task   → Receive() → handle → Reply()
 */

#define SUBJECT_MAX_OBS 16   /* max observers per subject */

/* ---- Observer "interface" ---- */
typedef struct Observer Observer;
struct Observer {
    void (*update)(Observer *self, int event, int value);
};

/* ---- Subject (Observable) ---- */
typedef struct {
    Observer *obs[SUBJECT_MAX_OBS];
    int       count;
} Subject;

void subject_init  (Subject *s);
int  subject_attach(Subject *s, Observer *o);  /* 0 = ok, -1 = full/dup */
void subject_detach(Subject *s, Observer *o);
void subject_notify(Subject *s, int event, int value);

/* ---- Concrete observer: synchronous function callback ---- */
typedef struct {
    Observer base;    /* MUST be first */
    void    *ctx;     /* caller-supplied context, passed to fn unchanged */
    void   (*fn)(void *ctx, int event, int value);
} FnObserver;

void fn_observer_init(FnObserver *o,
                      void (*fn)(void *ctx, int event, int value),
                      void *ctx);

/* ---- Concrete observer: IPC cross-task notification ---- */

/* Message layout sent by TidObserver to the target task. */
typedef struct { int event; int value; } ObsMsg;

typedef struct {
    Observer base;   /* MUST be first */
    int      tid;    /* destination task ID */
} TidObserver;

void tid_observer_init(TidObserver *o, int tid);

#endif /* OBSERVER_H */

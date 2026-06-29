#ifndef EVENTBUS_H
#define EVENTBUS_H

/*
 * Layer 3 — Global Event Bus built on the GoF Observer pattern.
 *
 * Topics are named strings (e.g. "input:key", "clock:tick").
 * A publish() call synchronously invokes every registered observer for
 * that topic: FnObservers run a callback in the same task, TidObservers
 * IPC-Send an ObsMsg to a target task (which must be in Receive()).
 *
 * Usage:
 *
 *   // Publisher side (any task):
 *   eventbus_publish("input:key", EV_KEY_DOWN, keycode);
 *
 *   // Subscriber — synchronous callback (same task context as publisher):
 *   static void on_key(void *ctx, int event, int value) { ... }
 *   eventbus_subscribe_fn("input:key", on_key, NULL);
 *
 *   // Subscriber — cross-task IPC (target must be in Receive()):
 *   eventbus_subscribe_tid("clock:tick", my_task_tid);
 *   // target:
 *   int from; ObsMsg msg;
 *   Receive(&from, (char*)&msg, sizeof(msg));
 *   Reply(from, (char*)&(int){0}, sizeof(int));
 *
 *   // Unsubscribe:
 *   eventbus_detach_fn ("input:key", on_key, NULL);
 *   eventbus_detach_tid("clock:tick", my_task_tid);
 */

#include "observer.h"    /* Subject, FnObserver, TidObserver, ObsMsg */

#define EVENTBUS_MAX_TOPICS  16   /* max distinct topic names           */
#define EVENTBUS_MAX_FN_OBS  32   /* pool of synchronous observers      */
#define EVENTBUS_MAX_TID_OBS 32   /* pool of IPC observers              */

void eventbus_init(void);

/* Publish event+value to all observers of topic (push, synchronous).
 * topic must not be NULL; if unknown, call is silently ignored. */
void eventbus_publish(const char *topic, int event, int value);

/* Attach a synchronous function callback to topic.
 * Returns 0 on success, -1 if topic pool or observer pool is full. */
int eventbus_subscribe_fn (const char *topic,
                           void (*fn)(void *ctx, int event, int value),
                           void *ctx);

/* Attach an IPC observer: publish will Send(ObsMsg{event,value}) to tid.
 * The target task MUST be blocked in Receive() when the event fires.
 * Returns 0 on success, -1 if topic pool or observer pool is full. */
int eventbus_subscribe_tid(const char *topic, int tid);

/* Detach the first matching FnObserver (matched by fn AND ctx). */
void eventbus_detach_fn (const char *topic,
                         void (*fn)(void *ctx, int event, int value),
                         void *ctx);

/* Detach the first TidObserver whose tid matches. */
void eventbus_detach_tid(const char *topic, int tid);

#endif /* EVENTBUS_H */

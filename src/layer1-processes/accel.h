#ifndef ACCEL_H
#define ACCEL_H

/*
 * Accelerator-core dispatch API.
 *
 * Cores 1-3 run a bare WFE worker loop (secondary_worker_c) and are not
 * part of the OS scheduler.  Core 0 submits short work items with
 * accel_dispatch() and waits for results with accel_wait() / accel_wait_all().
 *
 * Each AccelJob is padded to one cache line (64 B) to prevent false sharing.
 * The fn pointer is the concurrency signal: NULL means idle, non-NULL means
 * a job is pending/running.  The worker clears fn after finishing and fires
 * SEV so accel_wait() can exit its WFE sleep.
 *
 * Primary use-cases: parallel canvas-band fill, and parallel screenbuf diff
 * (APU1–3 enqueue UART cell updates; CPU0 drains the queue).
 */

typedef void (*accel_fn_t)(void *arg);

typedef struct __attribute__((aligned(64))) {
    volatile accel_fn_t  fn;        /* NULL = idle; non-NULL = job to execute */
    void *volatile       arg;
    volatile unsigned    jobs_done; /* incremented after every completed job */
    char _pad[64 - 2 * sizeof(void *) - sizeof(unsigned)];
} AccelJob;

/* One slot per secondary core.  Index 0 = core 1, 1 = core 2, 2 = core 3. */
extern AccelJob accel_jobs[3];

/* Call once in kmain after releasing secondary cores from the spin-table. */
void accel_init(void);

/* Query helpers — safe to call from any task without holding a lock. */
int      accel_is_busy(int core_id);    /* 1 if core is currently executing a job */
unsigned accel_jobs_done(int core_id);  /* cumulative completed job count */

/* Dispatch fn(arg) to the given core (1, 2, or 3). */
void accel_dispatch(int core_id, accel_fn_t fn, void *arg);

/* Block until the specified core's job slot is idle. */
void accel_wait(int core_id);

/* Block until all three secondary cores are idle. */
void accel_wait_all(void);

/* Entry point for secondary cores — called from secondary_entry (assembly). */
void secondary_worker_c(int core_id);

#endif

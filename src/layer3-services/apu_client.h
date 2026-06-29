#ifndef APU_CLIENT_H
#define APU_CLIENT_H

/*
 * Per-core APU server client API — DarcyOS APU terminal build.
 *
 * Each secondary core (1–3) has its own named server (APU1_server … APU3_server).
 * Clients Send() a job; the server dispatches to its core, Yield()s via
 * AwaitEvent() until the APU fires a completion SGI, then Reply()s.
 *
 * APUDispatch(core, fn, arg)  — one job on the chosen core (blocking)
 * APUBatch(jobs, n)           — up to 3 jobs in parallel (one per core)
 */

#define APU_ANY  (-1)

typedef struct {
	void (*fn)(void *);     /* work item entry point */
	void  *arg;             /* opaque pointer passed to fn */
} ApuJob;

/* Dispatch one job. core_or_any is 1, 2, 3, or APU_ANY.
 * Returns the core id that ran the job (1..3), or negative on error. */
int APUDispatch(int core_or_any, void (*fn)(void *), void *arg);

/* Dispatch up to APU_BATCH_MAX (3) jobs in parallel. jobs[i] runs on core (i+1).
 * Returns the number of jobs dispatched, or negative on error. */
int APUBatch(const ApuJob *jobs, int n);

#endif

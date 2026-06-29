#include "accel.h"
#include "apu_completion.h"

AccelJob accel_jobs[3];

void accel_init(void)
{
    for (int i = 0; i < 3; i++) {
        accel_jobs[i].fn        = (accel_fn_t)0;
        accel_jobs[i].arg       = (void *)0;
        accel_jobs[i].jobs_done = 0;
    }
    __asm__ volatile("dsb sy" ::: "memory");
}

int accel_is_busy(int core_id)
{
    return accel_jobs[core_id - 1].fn != (accel_fn_t)0;
}

unsigned accel_jobs_done(int core_id)
{
    return accel_jobs[core_id - 1].jobs_done;
}

/*
 * Dispatch a job to a secondary core.
 *
 * Memory ordering:
 *   dmb ish before writing fn ensures arg is visible before the worker
 *   reads it.  dsb sy + sev wakes the WFE sleep and broadcasts to all cores
 *   in the inner-shareable domain.
 */
void accel_dispatch(int core_id, accel_fn_t fn, void *arg)
{
    AccelJob *job = &accel_jobs[core_id - 1];
    job->arg = arg;
    __asm__ volatile("dmb ish" ::: "memory");
    job->fn = fn;
    __asm__ volatile("dsb sy\n sev" ::: "memory");
}

/*
 * Spin-wait with WFE until a secondary core's job slot is idle (fn == NULL).
 *
 * The secondary core fires SEV + dsb after clearing fn, so this WFE exits
 * promptly.  The dmb ish on exit provides the load-acquire barrier needed to
 * observe any memory side-effects of the completed job.
 */
void accel_wait(int core_id)
{
    AccelJob *job = &accel_jobs[core_id - 1];
    while (job->fn)
        __asm__ volatile("wfe" ::: "memory");
    __asm__ volatile("dmb ish" ::: "memory");
}

void accel_wait_all(void)
{
    for (int i = 1; i <= 3; i++)
        accel_wait(i);
}

/*
 * Secondary-core worker loop.  Runs at EL2 (the EL the QEMU spin-table
 * firmware uses).  Interrupts are disabled by secondary_entry before this
 * is called, so there is no IRQ handling here.
 *
 * Protocol:
 *   1. WFE until fn is non-NULL (core 0 will fire SEV after writing fn).
 *   2. dmb ish: acquire barrier so arg is fresh.
 *   3. Execute fn(arg).
 *   4. dmb ish + clear fn: release barrier so core 0 sees the side-effects.
 *   5. dsb sy + sev: signal core 0 waiting in accel_wait().
 */
void secondary_worker_c(int core_id)
{
    int idx = core_id - 1;
    AccelJob *job = &accel_jobs[idx];

    for (;;) {
        while (!job->fn)
            __asm__ volatile("wfe" ::: "memory");
        __asm__ volatile("dmb ish" ::: "memory");

        accel_fn_t fn = job->fn;
        void      *arg = job->arg;

        fn(arg);

        __asm__ volatile("dmb ish" ::: "memory");
        job->jobs_done++;
        job->fn = (accel_fn_t)0;
        __asm__ volatile("dsb sy\n sev" ::: "memory");
        apu_signal_completion(core_id);
    }
}

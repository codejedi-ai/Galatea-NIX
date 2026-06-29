#ifndef APU_COMPLETION_H
#define APU_COMPLETION_H

/*
 * APU completion events — one SGI per secondary core.
 *
 * Inter-core interrupt model (Pi 4 / GICv2):
 *   Core 0 → Core N : NOT an interrupt. accel_dispatch() writes shared
 *                     accel_jobs[N] and fires SEV (WFE wakeup).
 *   Core N → Core 0 : GIC Software-Generated Interrupt (SGI 1/2/3).
 *                     secondary_worker_c calls gic_send_sgi() after fn() returns.
 *                     CPU0 IRQ handler unblocks APU{N}_server in AwaitEvent().
 *
 * Only CPU0 runs the scheduler and takes IRQs. Cores 1–3 have DAIFSet (all
 * IRQs masked) and never enter HandleASYNC.
 */

#define APU1_DONE_EVENT  1   /* GIC SGI 1 → core 0 */
#define APU2_DONE_EVENT  2   /* GIC SGI 2 → core 0 */
#define APU3_DONE_EVENT  3   /* GIC SGI 3 → core 0 */

/* Map core id (1..3) to the AwaitEvent / SGI id. */
int apu_done_event(int core_id);

/* Called from secondary_worker_c after a job completes — fires SGI to CPU0. */
void apu_signal_completion(int core_id);

/* Enable and route the three APU completion SGIs to CPU0. Call from kmain. */
void apu_completion_init(void);

#endif

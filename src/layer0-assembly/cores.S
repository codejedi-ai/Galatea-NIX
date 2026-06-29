/*
 * Core utilities.  QEMU raspi4b exposes 4 cores; secondary_park halts cores
 * 1-3 in a minimal WFI loop.  secondary_entry replaces secondary_park when
 * the accelerator subsystem (accel.c) is active: it sets up a C stack for
 * each secondary core and calls secondary_worker_c() so those cores can
 * execute dispatched work items (see accel.h / accel.c).
 */

.section ".text"
.align 4

/* Park an unused core: mask all interrupts and halt in WFI forever. */
.global secondary_park
secondary_park:
	msr DAIFSet, #0xf
1:	wfi
	b	1b

/*
 * Accelerator entry point.  Written to the QEMU raspi4b spin-table mailboxes
 * (0xE0 / 0xE8 / 0xF0) instead of secondary_park when accel_init() is used.
 *
 * At this point the core is at EL2, SP is undefined.  We:
 *   1. Disable all interrupts (secondary cores never handle IRQs).
 *   2. Read the core number from MPIDR_EL1 (Aff0 = bits [7:0]).
 *   3. Assign a 16 KB C stack from the __cpu_stack_base linker region:
 *         sp = __cpu_stack_base + core_id * 0x4000
 *      (core 1 → base+0x4000, core 2 → base+0x8000, core 3 → base+0xC000)
 *   4. Call secondary_worker_c(core_id) — never returns.
 */
.global secondary_entry
secondary_entry:
	msr DAIFSet, #0xf        /* disable all IRQ/FIQ/SError/Debug */

	mrs x0, mpidr_el1
	and x0, x0, #0xff        /* Aff0 = core number (1, 2, or 3)  */

	/* sp = __cpu_stack_base + core_id * 0x4000 */
	ldr x1, =__cpu_stack_base
	mov x2, #0x4000
	mul x3, x0, x2
	add sp, x1, x3

	bl  secondary_worker_c   /* secondary_worker_c(core_id) — noreturn */

	/* Should never reach here. */
2:	wfi
	b   2b

/* Wake cores waiting in WFE (no-op effect in single-CPU, kept for the API). */
.global smp_send_event
smp_send_event:
	dsb sy
	sev
	ret

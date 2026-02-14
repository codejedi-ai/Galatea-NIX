/*
 * Layer 0: Spinlock acquire/release (ARM64 LDXR/STXR, STLR).
 * No inline ASM in C; spinlock primitives live here.
 */

.text
.align 4

.global spinlock_acquire
spinlock_acquire:
	/* x0 = lock pointer (volatile int *) */
1:	ldaxr	w1, [x0]
	cbnz	w1, 1b
	mov	w1, #1
	stlxr	w2, w1, [x0]
	cbnz	w2, 1b
	ret

.global spinlock_release
spinlock_release:
	/* x0 = lock pointer */
	stlr	wzr, [x0]
	ret

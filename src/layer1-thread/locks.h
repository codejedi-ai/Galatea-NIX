#ifndef _LOCKS_H_
#define _LOCKS_H_ 1

/*
 * Layer 1: Spinlock for short critical sections.
 * Thread spins until lock is acquired (ARM64 LDXR/STXR in layer0).
 */

#include <stdint.h>

typedef volatile int spinlock_t;

void SpinLock_Acquire(spinlock_t *lock);
void SpinLock_Release(spinlock_t *lock);

#endif

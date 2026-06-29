/*
 * Layer 1: Spinlock wrapper; primitives in layer0-assembly/spinlock.S.
 */

#include "locks.h"
#include "layer0.h"

void SpinLock_Acquire(spinlock_t *lock)
{
	spinlock_acquire(lock);
}

void SpinLock_Release(spinlock_t *lock)
{
	spinlock_release(lock);
}

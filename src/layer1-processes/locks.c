/*
 * Layer 1: Lock API — no-op stubs for single-CPU build.
 */

#include "locks.h"
#include "config.h"

void SpinLock_Acquire(spinlock_t *lock)
{
	(void)lock;
}

int SpinLock_TryAcquire(spinlock_t *lock)
{
	(void)lock;
	return 1;
}

void SpinLock_Release(spinlock_t *lock)
{
	(void)lock;
}

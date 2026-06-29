#ifndef _TASK_HEAP_H_
#define _TASK_HEAP_H_

#include <stddef.h>

/*
 * Per-task heap allocation (Layer 1).
 *
 * Each task gets its own 256 KB heap allocated from a separate PMM pool.
 * Isolation is logical: each task's malloc() draws from its own slice.
 */

void task_heap_init(void);
void task_set_current(int tid);      /* call during context switch */
void *task_malloc(size_t size);
void  task_free(void *ptr);
size_t task_heap_used(int tid);      /* bytes used by task tid */
size_t task_heap_free(int tid);      /* bytes free in task tid */

#endif

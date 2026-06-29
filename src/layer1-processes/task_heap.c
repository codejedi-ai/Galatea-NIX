#include "task_heap.h"
#include "config.h"
#include "pmm.h"
#include "rpi.h"

static struct {
    void   *phys_base;   /* page-aligned physical address from PMM */
    size_t  capacity;    /* total bytes (HEAP_SIZE_PER_TASK) */
    size_t  brk_offset;  /* bytes allocated (bump cursor) */
} task_heaps[NUMPROCS];

static int current_tid = -1;

void task_heap_init(void)
{
    size_t pages_per_task = (HEAP_SIZE_PER_TASK + PAGE_SIZE - 1) / PAGE_SIZE;

    for (int i = 0; i < NUMPROCS; i++) {
        void *phys = pmm_alloc_pages(pages_per_task);
        if (!phys) {
            uart_printf(CONSOLE,
                "[task_heap] WARN: pmm_alloc_pages(%u) failed for task %d\r\n",
                (unsigned)pages_per_task, i);
            task_heaps[i].phys_base  = 0;
            task_heaps[i].capacity   = 0;
            task_heaps[i].brk_offset = 0;
            continue;
        }
        task_heaps[i].phys_base  = phys;
        task_heaps[i].capacity   = HEAP_SIZE_PER_TASK;
        task_heaps[i].brk_offset = 0;
    }

    uart_printf(CONSOLE,
        "[task_heap] %d tasks × %u KB = %u KB reserved from PMM (logical)\r\n",
        NUMPROCS,
        (unsigned)(HEAP_SIZE_PER_TASK / 1024),
        (unsigned)(NUMPROCS * HEAP_SIZE_PER_TASK / 1024));
}

void task_set_current(int tid)
{
    if (tid >= 0 && tid < NUMPROCS)
        current_tid = tid;
}

void *task_malloc(size_t size)
{
    if (current_tid < 0 || current_tid >= NUMPROCS)
        return 0;
    if (!task_heaps[current_tid].phys_base)
        return 0;

    size = (size + 7) & ~(size_t)7;

    if (task_heaps[current_tid].brk_offset + size > task_heaps[current_tid].capacity)
        return 0;

    size_t off = task_heaps[current_tid].brk_offset;
    task_heaps[current_tid].brk_offset += size;
    return (char *)task_heaps[current_tid].phys_base + off;
}

void task_free(void *ptr)
{
    (void)ptr;
}

size_t task_heap_used(int tid)
{
    if (tid < 0 || tid >= NUMPROCS) return 0;
    return task_heaps[tid].brk_offset;
}

size_t task_heap_free(int tid)
{
    if (tid < 0 || tid >= NUMPROCS) return 0;
    return task_heaps[tid].capacity - task_heaps[tid].brk_offset;
}

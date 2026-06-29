#ifndef _pmm_h_
#define _pmm_h_ 1

#include <stdint.h>
#include <stddef.h>

/*
 * Physical frame allocator (PMM).
 *
 * Hands out 4 KB page frames from a fixed RAM pool (a page-aligned BSS arena).
 * This is the foundation the virtual-memory layer (kmmap/ksbrk) is built on,
 * and ultimately what a libc `mmap`/`brk` syscall would draw from.
 *
 * A bitmap tracks which frames are in use. Single-arena, no NUMA, no zones --
 * deliberately minimal (see docs/OS_ROADMAP.md).
 */

void   pmm_init(void);

/* Allocate one zeroed 4 KB frame. Returns a page-aligned pointer, or NULL. */
void  *pmm_alloc_page(void);

/* Allocate `n` physically-contiguous frames (for mmap regions). NULL if none. */
void  *pmm_alloc_pages(size_t n);

/* Free a single frame previously returned by pmm_alloc_page. */
void   pmm_free_page(void *p);

/* Free `n` contiguous frames previously returned by pmm_alloc_pages. */
void   pmm_free_pages(void *p, size_t n);

/* Statistics (in frames). */
uint64_t pmm_total_pages(void);
uint64_t pmm_free_pages_count(void);

/* Pool bounds for sanity checks. */
uint64_t pmm_pool_base(void);
uint64_t pmm_pool_end(void);

#endif /* pmm.h */

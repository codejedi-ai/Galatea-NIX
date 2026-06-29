#ifndef _vmm_h_
#define _vmm_h_ 1

#include <stdint.h>
#include <stddef.h>

#define MMAP_FAILED ((void *)-1)

void  vm_init(void);

/* Map an anonymous region of at least `length` bytes (rounded up to pages).
 * Returns a page-aligned pointer, or MMAP_FAILED. */
void *kmmap(size_t length);

/* Unmap a region previously returned by kmmap. */
void  kmunmap(void *addr, size_t length);

/* Move the program break by `increment` bytes. Returns the PREVIOUS break,
 * or MMAP_FAILED on failure (POSIX sbrk semantics). */
void *ksbrk(int64_t increment);

/* Run a self-test of the PMM + mmap/sbrk. Prints results.
 * Returns 1 on success, 0 on failure. */
int   vm_selftest(void);

#endif /* vmm.h */

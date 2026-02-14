#ifndef _MALLOC_H_
#define _MALLOC_H_ 1

#include <stdint.h>
#include "config.h"

/*
 * Heap allocator: free-list with coalescing.
 * Size in "words" (8 bytes). Block layout: [size (8B)][next (8B)] then payload.
 * Call malloc_init_default() once at boot (e.g. from InitSys) before any mymalloc.
 * Heap lives in BSS (heap_arena) so it is in RAM on QEMU virt.
 */

void malloc_init(uint64_t heap_start, uint64_t heap_size);
void malloc_init_default(void);  /* use BSS arena; call from InitSys */

uint64_t *mymalloc(uint64_t size_in_words);
void myfree(uint64_t *ptr);

#endif

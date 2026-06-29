#ifndef _MALLOC_H_
#define _MALLOC_H_ 1

#include <stdint.h>
#include "config.h"

/*
 * Heap allocator: free-list with coalescing and defragmentation.
 *
 * - Every memory reservation has three properties: Process ID, VRAM address,
 *   Physical RAM address. Each process has its own virtual heap range; the map
 *   stores (process_id, vram_address, phys_address) so vram under a process is
 *   mapped to physical RAM.
 * - Size in "words" (8 bytes). Block layout: [size (8B)][next (8B)] then payload.
 * - Defragmentation: free list sorted by address; first fit = smallest address.
 * - Without MMU: malloc_for_process returns physical payload pointer so
 *   dereference works; the map still holds the triple for bookkeeping/MMU later.
 *
 * Bare metal: no C library heap; these override malloc/free for the kernel.
 * Call malloc_init_default() once at boot (e.g. from InitSys) before any alloc.
 * Heap lives in BSS (heap_arena) so it is in RAM on QEMU virt.
 */

void malloc_init(uint64_t heap_start, uint64_t heap_size);
void malloc_init_default(void);  /* use BSS arena; call from InitSys */

uint64_t *malloc(uint64_t size_in_words);  /* alloc for process 0 (init/kernel) */
uint64_t *malloc_for_process(int process_id, uint64_t size_in_words);
void free(uint64_t *ptr);
/* Resolve process's VRAM (payload) address to physical payload pointer. Returns NULL if not found. */
uint64_t *malloc_vram_to_phys(int process_id, uint64_t vram_payload);

#endif

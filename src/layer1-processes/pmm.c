/*
 * Physical frame allocator (PMM) -- bitmap over a fixed 4 KB-page RAM pool.
 *
 * The pool lives in BSS so it is in RAM on the Pi 4 (link base 0x80000).
 * We do NOT rely on BSS being pre-zeroed: pmm_init() clears the bitmap explicitly.
 */
#include "pmm.h"
#include "config.h"
#include "rpi.h"

/* The page pool. Page-aligned so every frame is 4 KB-aligned. */
static uint8_t pmm_pool[PMM_POOL_PAGES * PAGE_SIZE] __attribute__((aligned(PAGE_SIZE)));

/* One bit per frame: 1 = used, 0 = free. */
static uint8_t pmm_bitmap[(PMM_POOL_PAGES + 7) / 8];

static uint64_t pmm_used;   /* frames currently allocated */

static inline int bit_get(uint64_t i) { return (pmm_bitmap[i >> 3] >> (i & 7)) & 1; }
static inline void bit_set(uint64_t i) { pmm_bitmap[i >> 3] |= (uint8_t)(1u << (i & 7)); }
static inline void bit_clr(uint64_t i) { pmm_bitmap[i >> 3] &= (uint8_t)~(1u << (i & 7)); }

void pmm_init(void)
{
	for (uint64_t i = 0; i < sizeof(pmm_bitmap); i++)
		pmm_bitmap[i] = 0;
	pmm_used = 0;
}

/* Index <-> address helpers. */
static inline uint64_t idx_to_addr(uint64_t i) { return (uint64_t)pmm_pool + i * PAGE_SIZE; }
static inline int addr_in_pool(uint64_t a)
{
	return a >= (uint64_t)pmm_pool && a < (uint64_t)pmm_pool + sizeof(pmm_pool);
}
static inline uint64_t addr_to_idx(uint64_t a) { return (a - (uint64_t)pmm_pool) / PAGE_SIZE; }

static void zero_page(void *p)
{
	uint64_t *q = (uint64_t *)p;
	for (uint64_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++)
		q[i] = 0;
}

void *pmm_alloc_page(void)
{
	for (uint64_t i = 0; i < PMM_POOL_PAGES; i++) {
		if (!bit_get(i)) {
			bit_set(i);
			pmm_used++;
			void *p = (void *)idx_to_addr(i);
			zero_page(p);
			return p;
		}
	}
	return 0;  /* out of frames */
}

void *pmm_alloc_pages(size_t n)
{
	if (n == 0) return 0;
	if (n == 1) return pmm_alloc_page();

	/* First-fit scan for n contiguous free frames. */
	for (uint64_t start = 0; start + n <= PMM_POOL_PAGES; start++) {
		uint64_t k = 0;
		while (k < n && !bit_get(start + k))
			k++;
		if (k == n) {
			for (uint64_t j = 0; j < n; j++) {
				bit_set(start + j);
				zero_page((void *)idx_to_addr(start + j));
			}
			pmm_used += n;
			return (void *)idx_to_addr(start);
		}
		start += k;  /* skip the run we just rejected */
	}
	return 0;
}

void pmm_free_page(void *p)
{
	uint64_t a = (uint64_t)p;
	if (!addr_in_pool(a)) return;
	uint64_t i = addr_to_idx(a);
	if (bit_get(i)) {
		bit_clr(i);
		pmm_used--;
	}
}

void pmm_free_pages(void *p, size_t n)
{
	uint64_t a = (uint64_t)p;
	if (!addr_in_pool(a)) return;
	uint64_t i = addr_to_idx(a);
	for (uint64_t j = 0; j < n && (i + j) < PMM_POOL_PAGES; j++) {
		if (bit_get(i + j)) {
			bit_clr(i + j);
			pmm_used--;
		}
	}
}

uint64_t pmm_total_pages(void)      { return PMM_POOL_PAGES; }
uint64_t pmm_free_pages_count(void) { return PMM_POOL_PAGES - pmm_used; }
uint64_t pmm_pool_base(void)        { return (uint64_t)pmm_pool; }
uint64_t pmm_pool_end(void)         { return (uint64_t)pmm_pool + sizeof(pmm_pool); }

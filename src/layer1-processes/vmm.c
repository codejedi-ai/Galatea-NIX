#include "vmm.h"
#include "pmm.h"
#include "config.h"
#include "rpi.h"
#include "apu_completion.h"

/* ----------------------------------------------------------- mmap / sbrk --- */

static inline size_t pages_for(size_t length)
{
	return (length + PAGE_SIZE - 1) / PAGE_SIZE;
}

void *kmmap(size_t length)
{
	if (length == 0) return MMAP_FAILED;
	void *p = pmm_alloc_pages(pages_for(length));
	return p ? p : MMAP_FAILED;
}

void kmunmap(void *addr, size_t length)
{
	if (!addr || addr == MMAP_FAILED || length == 0) return;
	pmm_free_pages(addr, pages_for(length));
}

static uint64_t brk_base, brk_cur, brk_end;

void *ksbrk(int64_t increment)
{
	if (brk_base == 0) {
		void *r = pmm_alloc_pages(BRK_REGION_PAGES);
		if (!r) return MMAP_FAILED;
		brk_base = brk_cur = (uint64_t)r;
		brk_end  = brk_base + (uint64_t)BRK_REGION_PAGES * PAGE_SIZE;
	}
	uint64_t old = brk_cur;
	int64_t  nxt = (int64_t)brk_cur + increment;
	if (nxt < (int64_t)brk_base || nxt > (int64_t)brk_end)
		return MMAP_FAILED;
	brk_cur = (uint64_t)nxt;
	return (void *)old;
}

/* ----------------------------------------------------------------- init ---- */

void vm_init(void)
{
	pmm_init();
}

/* --------------------------------------------------------------- selftest -- */

int vm_selftest(void)
{
	int ok = 1;

	uint64_t *a = (uint64_t *)pmm_alloc_page();
	if (!a) { ok = 0; }
	else {
		a[0] = 0xCAFEBABEull; a[200] = 0x1234567890ABCDEFull;
		if (a[0] != 0xCAFEBABEull || a[200] != 0x1234567890ABCDEFull) ok = 0;
		pmm_free_page(a);
	}

	size_t len = 3 * PAGE_SIZE + 17;
	uint8_t *m = (uint8_t *)kmmap(len);
	if (m == (uint8_t *)MMAP_FAILED) { ok = 0; }
	else {
		m[0] = 0xAA; m[len - 1] = 0x55;
		if (m[0] != 0xAA || m[len - 1] != 0x55) ok = 0;
		kmunmap(m, len);
	}

	void *b0 = ksbrk(0);
	void *b1 = ksbrk(4096);
	void *b2 = ksbrk(-4096);
	if (b0 == MMAP_FAILED || b1 == MMAP_FAILED || b2 == MMAP_FAILED) ok = 0;
	else if (b1 != b0) ok = 0;
	else if ((uint8_t *)b2 - (uint8_t *)b0 != 4096) ok = 0;

	uart_printf(CONSOLE,
		"\033[1;32m[  OK  ]\033[0m Memory self-test: pmm=%u/%u frames free  %s\r\n",
		(unsigned)pmm_free_pages_count(), (unsigned)pmm_total_pages(),
		ok ? "PASS" : "\033[1;31mFAIL\033[0m");

	return ok;
}

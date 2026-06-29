/*
 * System timer: free-running counter and compare C3 for interrupts.
 * QEMU virt uses CNTPCT_EL0; RPi4 uses memory-mapped system timer.
 */

#include "systimer.h"
#include "mmio_config.h"

#define SYSTIME_CS   0x00
#define SYSTIME_CLO  0x04
#define SYSTIME_CHI  0x08
#define SYSTIME_C0   0x0C
#define SYSTIME_C1   0x10
#define SYSTIME_C2   0x14
#define SYSTIME_C3   0x18

static char *const SYSTIMER_BASE = (char *)(CONFIG_SYSTIMER_BASE);
#define SYSTIMER_REG(offset) (*(volatile uint32_t *)(SYSTIMER_BASE + (offset)))

uint32_t get_timerLO(void)
{
#if TARGET_QEMU_VIRT == 1
	uint64_t val;
	asm volatile("mrs %0, cntpct_el0" : "=r"(val));
	return (uint32_t)val;
#else
	return SYSTIMER_REG(SYSTIME_CLO);
#endif
}

uint32_t get_timerHI(void)
{
#if TARGET_QEMU_VIRT == 1
	uint64_t val;
	asm volatile("mrs %0, cntpct_el0" : "=r"(val));
	return (uint32_t)(val >> 32);
#else
	return SYSTIMER_REG(SYSTIME_CHI);
#endif
}

uint64_t get_timerFULL(void)
{
	uint64_t hi = get_timerHI();
	uint64_t lo = get_timerLO();
	return (hi << 32) + lo;
}

void set_timerC3(unsigned int value)
{
	SYSTIMER_REG(SYSTIME_C3) = value;
}

uint32_t get_timerC3(void)
{
	return SYSTIMER_REG(SYSTIME_C3);
}

void clear_timer_status(void)
{
	SYSTIMER_REG(SYSTIME_CS) = 0;
}

/*
 * System timer: BCM2711 memory-mapped system timer (RPi 4).
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
	return SYSTIMER_REG(SYSTIME_CLO);
}

uint32_t get_timerHI(void)
{
	return SYSTIMER_REG(SYSTIME_CHI);
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

/* ---- ARM generic timer (EL1 physical, CNTP). Interrupt = PPI 30. ---- */

uint64_t gentimer_freq(void)
{
	uint64_t f;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
	return f ? f : 1;
}

uint64_t gentimer_count(void)
{
	uint64_t c;
	__asm__ volatile("isb; mrs %0, cntpct_el0" : "=r"(c));
	return c;
}

/* Program the down-counter to fire in `ms` ms and enable (ENABLE=1, IMASK=0).
 * Writing CNTP_TVAL re-arms and clears the pending condition, so this both
 * starts and re-arms the tick on each interrupt. Per-CPU register. */
void gentimer_arm_ms(uint32_t ms)
{
	uint64_t tval = (gentimer_freq() * (uint64_t)ms) / 1000ull;
	__asm__ volatile("msr cntp_tval_el0, %0" :: "r"(tval));
	__asm__ volatile("msr cntp_ctl_el0, %0" :: "r"((uint64_t)1));
	__asm__ volatile("isb");
}

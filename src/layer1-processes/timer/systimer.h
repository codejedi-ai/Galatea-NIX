#ifndef _SYSTIMER_H_
#define _SYSTIMER_H_

#include <stdint.h>

uint32_t get_timerLO(void);
uint32_t get_timerHI(void);
uint64_t get_timerFULL(void);
void clear_timer_status(void);
void set_timerC3(unsigned int value);
uint32_t get_timerC3(void);

/* ARM generic timer (EL1 physical timer, CNTP). Its interrupt is PPI 30, which
 * QEMU raspi4b delivers reliably (unlike the BCM2711 system-timer compare IRQ).
 * This drives the kernel tick. get_timer* above remain the free-running counter
 * for runtime accounting (those MMIO reads work even though the compare IRQ does not). */
uint64_t gentimer_freq(void);      /* CNTFRQ_EL0 (ticks/sec) */
uint64_t gentimer_count(void);     /* CNTPCT_EL0 (free-running count) */
void gentimer_arm_ms(uint32_t ms); /* program + enable the next tick on THIS CPU */

#endif

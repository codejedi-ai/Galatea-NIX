#ifndef _SYSTIMER_H_
#define _SYSTIMER_H_

#include <stdint.h>

uint32_t get_timerLO(void);
uint32_t get_timerHI(void);
uint64_t get_timerFULL(void);
void clear_timer_status(void);
void set_timerC3(unsigned int value);
uint32_t get_timerC3(void);

#endif

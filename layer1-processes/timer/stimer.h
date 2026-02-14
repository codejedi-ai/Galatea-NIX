#ifndef _STIMER_H_
#define _STIMER_H_

#include <stdint.h>
#include <stddef.h>

uint32_t stimer_getlo(void);
uint32_t stimer_gethi(void);
void stimer_snooze(void);
void stimer_wake(void);
void stimer_creset(size_t line);

#endif

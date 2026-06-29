#ifndef CLOCKSERVER_H
#define CLOCKSERVER_H

#include <stdint.h>

#define CLOCK_SERVER_PRIORITY   5
#define CLOCK_NOTIFIER_PRIORITY 2
#define CLOCK_MAX_DELAY         64

void clock_server_entry(void);
void clock_notifier_entry(void);
uint32_t ClockServerTicks(void);
int ClockServerTid(void);   /* static handle; valid once the clock server has run */

#endif

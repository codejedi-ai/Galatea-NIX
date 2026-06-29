#include "rpi.h"

void gic_init(void);
void gic_cpu_init(void);
uint32_t checkActiveInterrupt(uint32_t interrupt_id);
void route_interrupt(uint32_t interrupt_id, uint8_t cpu_target);
void enable_interrupt(uint32_t interrupt_id);
uint32_t readInterruptId();
void clear_GICC_EOIR(uint16_t interrupt_id);
void gic_send_sgi(uint32_t sgi_id, uint8_t cpu_target_mask);
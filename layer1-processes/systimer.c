#include "rpi.h"
#include "util.h"
#include "mmio_config.h"
#include <stdarg.h>
# include "systimer.h"
static char* const  MMIO_BASE = (char*)           CONFIG_MMIO_BASE;
/*********** SYSTIMER CONTROL ************************ ************/

static char* const SYSTIMER_BASE = (char*)(CONFIG_SYSTIMER_BASE); // Using CONFIG_SYSTIMER_BASE for platform independence

// SYSTIME Register Offsets and Descriptions
// differnt control registers for the timer
// I wonder would the timer still run when the computer is held at a busy wait during user input. 
static const uint32_t SYSTIME_CS   = 0x00;   // System Timer Control/Status
static const uint32_t SYSTIME_CLO  = 0x04;   // System Timer Counter Lower 32 bits
static const uint32_t SYSTIME_CHI  = 0x08;   // System Timer Counter Higher 32 bits
static const uint32_t SYSTIME_C0   = 0x0C;   // System Timer Compare 0
static const uint32_t SYSTIME_C1   = 0x10;   // System Timer Compare 1
static const uint32_t SYSTIME_C2   = 0x14;   // System Timer Compare 2
static const uint32_t SYSTIME_C3   = 0x18;   // System Timer Compare 3

#define SYSTIMER_REG(offset) (*(volatile uint32_t*)(SYSTIMER_BASE + offset))
// read the mem location of timerbase plus the offset
// This is the timer value []

uint32_t get_timerLO() {
#if TARGET_QEMU_VIRT == 1
    // QEMU virt uses ARM architecturally-defined generic timers
    // Read the physical counter CNTPCT_EL0 (lower 32 bits)
    uint64_t val;
    asm volatile("mrs %0, cntpct_el0" : "=r" (val));
    return (uint32_t)val;
#else
    // RPi4 has memory-mapped system timer
    const unsigned int ret = SYSTIMER_REG(SYSTIME_CLO);
    return ret;
#endif
}
uint32_t get_timerHI(){
#if TARGET_QEMU_VIRT == 1
    // QEMU virt uses ARM architecturally-defined generic timers
    // Read the physical counter CNTPCT_EL0 (upper 32 bits)
    uint64_t val;
    asm volatile("mrs %0, cntpct_el0" : "=r" (val));
    return (uint32_t)(val >> 32);
#else
    // RPi4 has memory-mapped system timer
    const unsigned int ret = SYSTIMER_REG(SYSTIME_CHI);
    return ret;
#endif
}
uint64_t get_timerFULL(){
    uint64_t time = get_timerHI();
	time = time << 32;
	time += get_timerLO();
    return time;
}
// set C3 to a value such that the timer interrupt would fire if the value is equal to the current timer value
void set_timerC3(unsigned int value){
    SYSTIMER_REG(SYSTIME_C3) = value;
}
// get C3 value
uint32_t get_timerC3(){
    return SYSTIMER_REG(SYSTIME_C3);
}
void clear_timer_status(){
    SYSTIMER_REG(SYSTIME_CS) = 0;
}
// Any registers that can be set by the deivce is set as volitilelo / 1e6
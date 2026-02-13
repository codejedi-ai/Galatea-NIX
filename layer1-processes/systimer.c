#include "rpi.h"
#include "util.h"
#include <stdarg.h>
# include "systimer.h"
static char* const  MMIO_BASE = (char*)           0xFE000000;
/*********** SYSTIMER CONTROL ************************ ************/

static char* const SYSTIMER_BASE = (char*)(MMIO_BASE + 0x003000); // it is not 0x7E003000 replace every 0x7E we use 0xFE

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
    // Read the values from SYSTIME_CHI and SYSTIME_CLO
    const unsigned int ret = SYSTIMER_REG(SYSTIME_CLO);
    return ret;
}
uint32_t get_timerHI(){
    // Read the values from SYSTIME_CHI and SYSTIME_CLO
    const unsigned int ret = SYSTIMER_REG(SYSTIME_CHI);
    return ret;
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
void resetCS(uint32_t value){
    SYSTIMER_REG(SYSTIME_CS) = SYSTIMER_REG(SYSTIME_CS) & (1 << value);
}
// Any registers that can be set by the deivce is set as volitilelo / 1e6
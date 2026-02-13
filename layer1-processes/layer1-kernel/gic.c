#include "gic.h"
#define GIC_BASE 0xff840000


#define GICD_BASE GIC_BASE + 0x1000
// enable the interrupt, the set enable register
// #define GICD_ISENABLERn = GICD_BASE + 0x100
#define GICD_ISENABLERn GICD_BASE + 0x100
// route the interrupt to IRQ on CPU 0
// #define GICD_ITARGETSRn = GICD_BASE + 0x800
#define GICD_ITARGETSRn GICD_BASE + 0x800

#define GICC_BASE GIC_BASE + 0x2000
// GICD_GICC_IAR
#define GICC_IAR *(uint32_t*)(GICC_BASE + 0x0C)
// GICC_EOIR
#define GICC_EOIR *(uint32_t*)(GICC_BASE + 0x10)
// GICC_EOIR



#define GICD_ITARGETSR(n) (*(uint32_t*)(GICD_ITARGETSRn + (4 * n)))
// enable the interrupt use GICD_ISENABLERn 4-byte registers, with 1 bit per InterruptID
#define GICD_ISENABLER(n) (*(uint32_t*)(GICD_ISENABLERn + (4 * n)))
// set active interrupt
#define GICD_ISACTIVERn(n) (*(uint32_t*)(GICD_ISENABLERn + (4 * n)))
// clear active interrupt
#define GICD_ICACTIVERn(n) (*(uint32_t*)(GICD_ISENABLERn + (4 * n)))

# define DEBUG 1
/*
oute the interrupt to IRQ on CPU 0
use GICD_ITARGETSRn
each register defines targets for 4 interrupts
1. Find the register off set use the interrupt_id DIV 4
2. Get the remainder m which would be the byte offset
3. Then write the CPU target feild value into the corrusponding byte
write 0x01 to the byte, that is the 0th cpu
*/
void route_interrupt(uint32_t interrupt_id, uint8_t cpu_target){
    if (cpu_target > 7){
        return;
    }
    uint32_t offset = interrupt_id / 4; // n 
    uint32_t remainder = interrupt_id % 4;
    uint32_t target = ((0x01 << cpu_target) << (remainder << 3));
    // print target in binary
    # if DEBUG == 3
    for (int i = 31; i >= 0; i--){
        if (target & (0x01 << i)){
             uart_printf(CONSOLE, "1");
         } else {
             uart_printf(CONSOLE, "0");
         }
    }
    uart_printf(CONSOLE, "\r\n");
    uart_printf(CONSOLE, "&GICD_ITARGETSR(offset) = %x\r\n", &GICD_ITARGETSR(offset));
    uart_printf(CONSOLE, "&GICD_ITARGETSR(offset) = %x\r\n", GICD_ITARGETSRn + (4 * offset));
    # endif
    GICD_ITARGETSR(offset) = target;
}

/*
enable the interrupt
use GICD_ISENABLERn
4-byte registers, with 1 bit per InterruptID
For interrupt ID m, when DIV and MOD are the integer division and modulo operations:
• the corresponding GICD_ISENABLER number, n, is given by n = m DIV 32
• the offset of the required GICD_ISENABLER is (0x100 + (4*n))
• the bit number of the required Set-enable bit in this register is m MOD 32.
*/
void enable_interrupt(uint32_t interrupt_id){
    uint32_t offset = interrupt_id / 32; // n 
    uint32_t remainder = interrupt_id % 32;
    GICD_ISENABLER(offset) = GICD_ISENABLER(offset) | (0x01 << remainder);
}

// set active interrupt
void setActiveInterrupt(uint32_t interrupt_id){
    uint32_t offset = interrupt_id / 32; // n 
    uint32_t remainder = interrupt_id % 32;
    GICD_ISACTIVERn(offset) = GICD_ISACTIVERn(offset) | (0x01 << remainder);
}
// check active interrupt
uint32_t checkActiveInterrupt(uint32_t interrupt_id){
    uint32_t offset = interrupt_id / 32; // n 
    uint32_t remainder = interrupt_id % 32;
    return GICD_ISACTIVERn(offset) & (0x01 << remainder);
}
// clear active interrupt
void INTERRUPT_CLEAR_ACTIVE_REGS(uint32_t interrupt_id){
    uint32_t offset = interrupt_id / 32; // n 
    uint32_t remainder = interrupt_id % 32;
    GICD_ICACTIVERn(offset) = GICD_ICACTIVERn(offset) | (0x01 << remainder);
}

uint32_t readInterruptId(){
    return GICC_IAR & 0x3FF;
}
void clear_GICC_EOIR(uint16_t interrupt_id){
    if (interrupt_id > 1023){
        // print the error message mentioning the interrupt_id is out of range which is 0-1023
        uart_printf(CONSOLE, "Interrupt ID is out of range must be within 0 - 1023\r\n");
        return;
    }
    uint32_t toBeWritten = GICC_EOIR;
    toBeWritten = toBeWritten >> 10;
    toBeWritten = toBeWritten << 10;
    toBeWritten = toBeWritten | interrupt_id;
    // would only want to change the last 10 bits
    GICC_EOIR = interrupt_id;
}
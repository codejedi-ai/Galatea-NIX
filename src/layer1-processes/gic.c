#include "gic.h"
#include "mmio_config.h"

/* GICv2 on BCM2711 (Pi 4 / QEMU raspi4b). Offsets from mmio_config.h. */
#define GICD_REG(off, n) \
	MMIO_READ_REG(CONFIG_GICD_BASE, (off) + 4u * (uint32_t)(n))
#define GICD_REG_SET(off, n, val) \
	MMIO_WRITE_REG(CONFIG_GICD_BASE, (off) + 4u * (uint32_t)(n), (val))

# define DEBUG 1

/* Enable the GICv2 so interrupts are actually delivered. Without this the
 * distributor + CPU interface are off and the priority mask blocks everything,
 * so the timer IRQ never reaches the CPU. GICD_CTLR is global; GICC_CTLR/PMR
 * are per-CPU -- call gic_cpu_init() on each CPU that takes interrupts. */
void gic_cpu_init(void){
    MMIO_WRITE_REG(CONFIG_GICC_BASE, GICC_PMR, 0xFFu);   /* unmask all priorities */
    MMIO_WRITE_REG(CONFIG_GICC_BASE, GICC_CTLR, 0x1u);   /* enable CPU interface */
}

void gic_init(void){
    MMIO_WRITE_REG(CONFIG_GICD_BASE, GICD_CTLR, 0x1u);   /* enable distributor */
    gic_cpu_init();
}

void route_interrupt(uint32_t interrupt_id, uint8_t cpu_target){
    if (cpu_target > 7){
        return;
    }
    uint32_t offset = interrupt_id / 4;
    uint32_t remainder = interrupt_id % 4;
    uint32_t target = ((0x01u << cpu_target) << (remainder << 3));
    # if DEBUG == 3
    for (int i = 31; i >= 0; i--){
        if (target & (0x01u << (unsigned)i)){
             uart_printf(CONSOLE, "1");
         } else {
             uart_printf(CONSOLE, "0");
         }
    }
    uart_printf(CONSOLE, "\r\n");
    uart_printf(CONSOLE, "GICD_ITARGETSR offset %u\r\n", offset);
    # endif
    GICD_REG_SET(GICD_ITARGETSR, offset, target);
}

void enable_interrupt(uint32_t interrupt_id){
    uint32_t offset = interrupt_id / 32;
    uint32_t remainder = interrupt_id % 32;
    uint32_t cur = GICD_REG(GICD_ISENABLER0, offset);
    GICD_REG_SET(GICD_ISENABLER0, offset, cur | (0x01u << remainder));
}

void setActiveInterrupt(uint32_t interrupt_id){
    uint32_t offset = interrupt_id / 32;
    uint32_t remainder = interrupt_id % 32;
    uint32_t cur = GICD_REG(GICD_ISENABLER0, offset);
    GICD_REG_SET(GICD_ISENABLER0, offset, cur | (0x01u << remainder));
}

uint32_t checkActiveInterrupt(uint32_t interrupt_id){
    uint32_t offset = interrupt_id / 32;
    uint32_t remainder = interrupt_id % 32;
    return GICD_REG(GICD_ISENABLER0, offset) & (0x01u << remainder);
}

void INTERRUPT_CLEAR_ACTIVE_REGS(uint32_t interrupt_id){
    uint32_t offset = interrupt_id / 32;
    uint32_t remainder = interrupt_id % 32;
    uint32_t cur = GICD_REG(GICD_ISENABLER0, offset);
    GICD_REG_SET(GICD_ISENABLER0, offset, cur | (0x01u << remainder));
}

uint32_t readInterruptId(void){
    return MMIO_READ_REG(CONFIG_GICC_BASE, GICC_IAR) & 0x3FFu;
}

void clear_GICC_EOIR(uint16_t interrupt_id){
    if (interrupt_id > 1023u){
        uart_printf(CONSOLE, "Interrupt ID is out of range must be within 0 - 1023\r\n");
        return;
    }
    MMIO_WRITE_REG(CONFIG_GICC_BASE, GICC_EOIR, (uint32_t)interrupt_id);
}

/* Software-generated interrupt (GICv2 SGIR). cpu_target_mask bit n → CPU n. */
void gic_send_sgi(uint32_t sgi_id, uint8_t cpu_target_mask)
{
    if (sgi_id > 15u)
        return;
    uint32_t val = ((uint32_t)cpu_target_mask << 16) | (sgi_id & 0xFu);
    MMIO_WRITE_REG(CONFIG_GICD_BASE, 0xF00u, val);
}

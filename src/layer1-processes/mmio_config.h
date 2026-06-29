#ifndef MMIO_CONFIG_H
#define MMIO_CONFIG_H

/**
 * BCM2711 (Raspberry Pi 4) memory-mapped I/O — used on hardware and QEMU raspi4b.
 * Kernel links at 0x80000; peripherals in the 0xFE000000 window.
 */

#define CONFIG_PLATFORM_NAME "Raspberry Pi 4 (BCM2711)"

#define CONFIG_MMIO_BASE         0xFE000000
#define CONFIG_UART0_BASE        (CONFIG_MMIO_BASE + 0x201000)
#define CONFIG_UART3_BASE        (CONFIG_MMIO_BASE + 0x201600)
/* BCM2835 AUX block (mini-UART). On QEMU raspi4b this is serial1 — the second
 * `-serial`, used as the host/container link. QEMU does NOT model UART2-5; the
 * UART3 "Marklin" line above only exists on real Pi 4 hardware. */
#define CONFIG_AUX_BASE          (CONFIG_MMIO_BASE + 0x215000)
#define CONFIG_GIC_BASE          0xFF840000
#define CONFIG_GICD_BASE         (CONFIG_GIC_BASE + 0x1000)
#define CONFIG_GICC_BASE         (CONFIG_GIC_BASE + 0x2000)
#define CONFIG_SYSTIMER_BASE     (CONFIG_MMIO_BASE + 0x003000)
#define CONFIG_GPIO_BASE         (CONFIG_MMIO_BASE + 0x200000)
#define CONFIG_STIMER_BASE       (CONFIG_MMIO_BASE + 0x003000)

#define UART_DR   0x00
#define UART_FR   0x18
#define UART_IBRD 0x24
#define UART_FBRD 0x28
#define UART_LCRH 0x2C
#define UART_CR   0x30
#define UART_IMSC 0x38
#define UART_RIS  0x3C
#define UART_ICR  0x44

#define UART_FR_CTS   (1 << 0)
#define UART_FR_DSR   (1 << 1)
#define UART_FR_DCD   (1 << 2)
#define UART_FR_BUSY  (1 << 3)
#define UART_FR_RXFE  (1 << 4)
#define UART_FR_TXFF  (1 << 5)
#define UART_FR_RXFF  (1 << 6)
#define UART_FR_TXFE  (1 << 7)

#define UART_CR_UARTEN   0x001
#define UART_CR_LBE      0x080
#define UART_CR_TXE      0x100
#define UART_CR_RXE      0x200
#define UART_CR_DTR      0x400
#define UART_CR_RTS      0x800
#define UART_CR_OUT1     0x1000
#define UART_CR_OUT2     0x2000
#define UART_CR_RTSEN    0x4000
#define UART_CR_CTSEN    0x8000

#define UART_LCRH_PEN        0x002
#define UART_LCRH_EPS        0x004
#define UART_LCRH_STP2       0x008
#define UART_LCRH_FEN        0x010
#define UART_LCRH_WLEN_LOW   0x020
#define UART_LCRH_WLEN_HIGH  0x040
#define UART_LCRH_SPS        0x080

#define GICD_CTLR        0x000   /* distributor control (enable) */
#define GICD_ISENABLER0  0x100
#define GICD_ITARGETSR   0x800
#define GICC_CTLR        0x000   /* CPU interface control (enable) */
#define GICC_PMR         0x004   /* CPU interface priority mask */
#define GICC_IAR         0x0C
#define GICC_EOIR        0x10

#define MMIO_READ32(addr)        (*(volatile uint32_t*)(addr))
#define MMIO_WRITE32(addr, val)  (*(volatile uint32_t*)(addr) = (val))
#define MMIO_READ_REG(base, off) MMIO_READ32((char*)(base) + (off))
#define MMIO_WRITE_REG(base, off, val) MMIO_WRITE32((char*)(base) + (off), (val))

#endif

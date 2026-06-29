#ifndef MMIO_CONFIG_H
#define MMIO_CONFIG_H

/**
 * MMIO Configuration Header
 * 
 * This file contains platform-agnostic definitions for all memory-mapped I/O addresses.
 * Set TARGET_QEMU_VIRT to 1 for QEMU virt, 0 for Raspberry Pi 4 hardware.
 */

// ============================================================================
// PLATFORM SELECTION
// ============================================================================
#ifndef TARGET_QEMU_VIRT
#define TARGET_QEMU_VIRT 1  // 1 for QEMU virt, 0 for RPi4
#endif

// ============================================================================
// QEMU VIRT MACHINE MEMORY MAP
// ============================================================================
#if TARGET_QEMU_VIRT == 1
  #define CONFIG_PLATFORM_NAME "QEMU virt (ARM Cortex-A72)"
  #define CONFIG_MMIO_BASE         0x09000000  // UART base
  #define CONFIG_UART0_BASE        0x09000000  // PL011 UART 0
  #define CONFIG_UART3_BASE        0x09000000  // PL011 UART 1 (same, line-based)
  #define CONFIG_GIC_BASE          0x08000000  // GICv2 base
  #define CONFIG_GICD_BASE         0x08000000  // GICv2 Distributor
  #define CONFIG_GICC_BASE         0x08010000  // GICv2 CPU Interface
  #define CONFIG_SYSTIMER_BASE     0x0F000000  // System Timer
  #define CONFIG_GPIO_BASE         (CONFIG_MMIO_BASE + 0x200000)  
  #define CONFIG_STIMER_BASE       (CONFIG_MMIO_BASE + 0x003000)

// ============================================================================
// RASPBERRY PI 4 HARDWARE MEMORY MAP
// ============================================================================
#else
  #define CONFIG_PLATFORM_NAME "Raspberry Pi 4 (BCM2711)"
  #define CONFIG_MMIO_BASE         0xFE000000  // Peripheral base
  #define CONFIG_UART0_BASE        (CONFIG_MMIO_BASE + 0x201000)  // UART 0
  #define CONFIG_UART3_BASE        (CONFIG_MMIO_BASE + 0x201600)  // UART 3
  #define CONFIG_GIC_BASE          0xFF840000  // GICv2 base
  #define CONFIG_GICD_BASE         (CONFIG_GIC_BASE + 0x1000)  // Distributor
  #define CONFIG_GICC_BASE         (CONFIG_GIC_BASE + 0x2000)  // CPU Interface
  #define CONFIG_SYSTIMER_BASE     (CONFIG_MMIO_BASE + 0x003000)  // System Timer
  #define CONFIG_GPIO_BASE         (CONFIG_MMIO_BASE + 0x200000)  
  #define CONFIG_STIMER_BASE       (CONFIG_MMIO_BASE + 0x003000)
#endif

// ============================================================================
// UART REGISTER OFFSETS (Platform-independent)
// ============================================================================
#define UART_DR   0x00   // Data Register
#define UART_FR   0x18   // Flag Register
#define UART_IBRD 0x24   // Integer Baud Rate Divisor
#define UART_FBRD 0x28   // Fractional Baud Rate Divisor
#define UART_LCRH 0x2C   // Line Control Register (High byte)
#define UART_CR   0x30   // Control Register
#define UART_IMSC 0x38   // Interrupt Mask Set/Clear Register
#define UART_RIS  0x3C   // Raw Interrupt Status Register
#define UART_ICR  0x44   // Interrupt Clear Register

// ============================================================================
// UART FLAG REGISTER BITS (FR field values)
// ============================================================================
#define UART_FR_CTS   (1 << 0)   // Clear To Send
#define UART_FR_DSR   (1 << 1)   // Data Set Ready
#define UART_FR_DCD   (1 << 2)   // Data Carrier Detect
#define UART_FR_BUSY  (1 << 3)   // UART Busy
#define UART_FR_RXFE  (1 << 4)   // Receive FIFO Empty
#define UART_FR_TXFF  (1 << 5)   // Transmit FIFO Full
#define UART_FR_RXFF  (1 << 6)   // Receive FIFO Full
#define UART_FR_TXFE  (1 << 7)   // Transmit FIFO Empty

// ============================================================================
// UART CONTROL REGISTER BITS (CR field values)
// ============================================================================
#define UART_CR_UARTEN   0x001    // UART Enable
#define UART_CR_LBE      0x080    // Loopback Enable
#define UART_CR_TXE      0x100    // Transmit Enable
#define UART_CR_RXE      0x200    // Receive Enable
#define UART_CR_DTR      0x400    // Data Terminal Ready
#define UART_CR_RTS      0x800    // Request To Send
#define UART_CR_OUT1     0x1000   // Output 1
#define UART_CR_OUT2     0x2000   // Output 2
#define UART_CR_RTSEN    0x4000   // RTS Hardware Flow Control Enable
#define UART_CR_CTSEN    0x8000   // CTS Hardware Flow Control Enable

// ============================================================================
// UART LINE CONTROL REGISTER H BITS (LCRH field values)
// ============================================================================
#define UART_LCRH_PEN        0x002    // Parity Enable
#define UART_LCRH_EPS        0x004    // Even Parity Select
#define UART_LCRH_STP2       0x008    // Two Stop Bits Select  
#define UART_LCRH_FEN        0x010    // FIFO Enable
#define UART_LCRH_WLEN_LOW   0x020    // Word Length bit 0
#define UART_LCRH_WLEN_HIGH  0x040    // Word Length bit 1
#define UART_LCRH_SPS        0x080    // Stick Parity Select

// ============================================================================
// GIC REGISTER OFFSETS (GICv2)
// ============================================================================
#define GICD_ISENABLER0  0x100  // Interrupt Set-Enable Register 0
#define GICD_ITARGETSR   0x800  // Interrupt Target Register
#define GICC_IAR         0x0C   // Interrupt Acknowledge Register
#define GICC_EOIR        0x10   // End of Interrupt Register

// ============================================================================
// HELPER MACROS FOR REGISTER ACCESS
// ============================================================================
#define MMIO_READ32(addr)        (*(volatile uint32_t*)(addr))
#define MMIO_WRITE32(addr, val)  (*(volatile uint32_t*)(addr) = (val))
#define MMIO_READ_REG(base, off) MMIO_READ32((char*)(base) + (off))
#define MMIO_WRITE_REG(base, off, val) MMIO_WRITE32((char*)(base) + (off), (val))

#endif  // MMIO_CONFIG_H

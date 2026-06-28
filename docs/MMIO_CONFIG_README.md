# Non-Hardcoded MMIO Configuration System

## Overview

This kernel now uses a centralized, platform-agnostic memory mapping configuration system instead of hardcoded memory addresses. This eliminates the need to manually edit driver code when switching between **QEMU virt** and **Raspberry Pi 4** platforms.

## Files Modified

### Core Configuration File
- **[layer1-processes/mmio_config.h](layer1-processes/mmio_config.h)** - Centralized memory address definitions for all supported platforms

### Driver Files Updated  
- **[layer1-processes/rpi.c](layer1-processes/rpi.c)** - UART and GPIO drivers
  - Removed hardcoded 0x09000000 addresses
  - Now uses `CONFIG_UART0_BASE` and `CONFIG_UART3_BASE` from mmio_config.h
  - Removed duplicate register offset and flag definitions

- **[layer1-processes/gic.c](layer1-processes/gic.c)** - ARM GICv2 interrupt controller
  - Replaced hardcoded 0x08000000 and 0xff840000 addresses
  - Now uses platform-selected base addresses from mmio_config.h

- **[layer1-processes/systimer.c](layer1-processes/systimer.c)** - System timer
  - Replaced hardcoded 0xFE000000 (RPi4) / 0x09000000 (QEMU) addresses
  - Now uses `CONFIG_SYSTIMER_BASE` and `CONFIG_STIMER_BASE`

### Build Configuration
- **[Makefile](Makefile)** - Updated with platform selection support

## Building for Different Platforms

### QEMU Virt Machine (Default)
```bash
make clean
make
# or explicitly:
make PLATFORM=QEMU_VIRT
```

### Raspberry Pi 4 Hardware
```bash
make clean
make PLATFORM=RPI4
```

## Platform Memory Maps

### QEMU virt (Cortex-A72)
```
UART:  0x09000000 (PL011)
GIC:   0x08000000 (Distributor), 0x08010000 (CPU Interface)
Timer: 0x0F000000 (System Timer)
```

### Raspberry Pi 4 (BCM2711)
```
UART0: 0xFE201000 (PL011)
UART3: 0xFE201600 (PL011)
GIC:   0xFF840000 (base), +0x1000 (Distributor), +0x2000 (CPU Interface)
Timer: 0xFE003000 (System Timer)
```

## Configuration Details

### Platform Selection Mechanism

The system uses compile-time configuration via the `TARGET_QEMU_VIRT` preprocessor define:

```c
// In mmio_config.h:
#ifndef TARGET_QEMU_VIRT
#define TARGET_QEMU_VIRT 1  // 1 for QEMU virt, 0 for RPi4
#endif

#if TARGET_QEMU_VIRT == 1
  #define CONFIG_UART0_BASE 0x09000000
  // ... QEMU addresses ...
#else
  #define CONFIG_UART0_BASE (CONFIG_MMIO_BASE + 0x201000)
  // ... RPi4 addresses ...
#endif
```

### Available Configuration Constants

#### Base Addresses
- `CONFIG_MMIO_BASE` - Peripheral base address
- `CONFIG_UART0_BASE` - Primary UART
- `CONFIG_UART3_BASE` - Secondary UART
- `CONFIG_GIC_BASE` - GIC base
- `CONFIG_GICD_BASE` - GIC Distributor
- `CONFIG_GICC_BASE` - GIC CPU Interface
- `CONFIG_SYSTIMER_BASE` - System timer
- `CONFIG_GPIO_BASE` - GPIO base

#### Register Offsets (Platform-independent)
- `UART_DR` - Data Register (0x00)
- `UART_FR` - Flag Register (0x18)
- `UART_CR` - Control Register (0x30)
- `UART_LCRH` - Line Control Register (0x2C)
- `UART_IBRD` - Integer Baud Rate (0x24)
- `UART_FBRD` - Fractional Baud Rate (0x28)
- `UART_IMSC` - Interrupt Mask (0x38)
- `UART_RIS` - Raw Interrupt Status (0x3C)
- `UART_ICR` - Interrupt Clear (0x44)

#### Control Flag Bits
- `UART_FR_TXFF` - Transmit FIFO Full
- `UART_FR_RXFE` - Receive FIFO Empty
- `UART_CR_UARTEN` - UART Enable (0x001)
- `UART_CR_TXE` - Transmit Enable (0x100)
- `UART_CR_RXE` - Receive Enable (0x200)
- `UART_LCRH_FEN` - FIFO Enable (0x010)
- `UART_LCRH_WLEN_HIGH` - Word Length High (0x040)
- `UART_LCRH_WLEN_LOW` - Word Length Low (0x020)
- `UART_LCRH_STP2` - Two Stop Bits (0x008)

#### Helper Macros
```c
MMIO_READ32(addr)              // Read 32-bit value
MMIO_WRITE32(addr, val)        // Write 32-bit value
MMIO_READ_REG(base, offset)    // Read register at base + offset
MMIO_WRITE_REG(base, off, val) // Write register at base + offset
```

## Usage Example

### Before (Hardcoded)
```c
// In rpi.c - hardcoded for QEMU:
static char* const UART0_BASE = (char*)(0x09000000);
static char* const UART3_BASE = (char*)(MMIO_BASE + 0x201600);  // Wrong for QEMU!

// In gic.c - hardcoded mixing both platforms:
#define GIC_BASE 0x08000000  // QEMU, but comment says RPi4
#define GICC_BASE 0x08010000 // QEMU
```

### After (Platform-Agnostic)
```c
// In rpi.c:
#include "mmio_config.h"
static char* const UART0_BASE = (char*)(CONFIG_UART0_BASE);
static char* const UART3_BASE = (char*)(CONFIG_UART3_BASE);

// In gic.c:  
#include "mmio_config.h"
#define GICD_BASE CONFIG_GICD_BASE
#define GICC_BASE CONFIG_GICC_BASE
```

## Build Commands Cheatsheet

```bash
# Default build (QEMU virt)
make

# Clean rebuild for QEMU
make clean && make PLATFORM=QEMU_VIRT

# Build for Raspberry Pi 4
make clean && make PLATFORM=RPI4

# Verify both platforms compile
make clean && make PLATFORM=QEMU_VIRT && echo SUCCESS || exit 1
make clean && make PLATFORM=RPI4 && echo SUCCESS || exit 1
```

## Implementation Details

### How It Works

1. **Compile-Time Selection**: The `TARGET_QEMU_VIRT` flag is set via `-DTARGET_QEMU_VIRT=1` or `-DTARGET_QEMU_VIRT=0` during compilation.

2. **Preprocessor Branching**: `mmio_config.h` uses `#if TARGET_QEMU_VIRT == 1` to select appropriate memory addresses.

3. **No Runtime Overhead**: All platform selection happens at compile time. The compiled kernel contains only the addresses for its target platform.

4. **Consistent Interface**: All drivers see the same macro names (`CONFIG_*`) regardless of platform, making the code platform-agnostic.

### Files That Include mmio_config.h
- `layer1-processes/rpi.c` - UART driver (includes at top)
- `layer1-processes/gic.c` - GIC driver (includes at top)
- `layer1-processes/systimer.c` - Timer driver (includes at top)

## Verification

The kernel has been verified to compile successfully for both platforms:
- ✅ `make PLATFORM=QEMU_VIRT` - 87 KB binary
- ✅ `make PLATFORM=RPI4` - 87 KB binary
- ✅ `make` (default) - 87 KB binary (QEMU_VIRT)

All configurations produce clean compilation with only a standard linker warning about LOAD segment permissions.

## Adding New Drivers

When adding new drivers that need memory-mapped I/O:

1. Add base address and register offset definitions to `mmio_config.h`
2. Include `#include "mmio_config.h"` in your driver
3. Use `CONFIG_*` macros for all platform-dependent addresses
4. Use `MMIO_READ_REG()` / `MMIO_WRITE_REG()` helpers for register access

Example:
```c
#include "mmio_config.h"

static void my_init() {
    // Write to a register
    MMIO_WRITE_REG(CONFIG_UART0_BASE, UART_LCRH, 
                   UART_LCRH_FEN | UART_LCRH_WLEN_HIGH | UART_LCRH_WLEN_LOW);
    
    // Read from a register
    uint32_t flags = MMIO_READ_REG(CONFIG_UART0_BASE, UART_FR);
}
```

## Future Platforms

To support a new platform (e.g., BBB, AllWinner, etc.):

1. Add a new `#elif` block to `mmio_config.h` with target addresses
2. Define a new platform constant (e.g., `TARGET_ALLWINNER`)
3. Update `Makefile` to support `make PLATFORM=ALLWINNER`
4. Possibly adjust some driver-specific code if hardware differs (e.g., different UART version)

This system scales cleanly to support many platforms without code duplication.

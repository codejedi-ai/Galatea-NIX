# Quick Reference: Platform Configuration

## The Problem (Before)
```c
// Hardcoded for QEMU, breaks on RPi4
static char* const UART0_BASE = (char*)(0x09000000);

// Or hardcoded for RPi4, breaks on QEMU
static char* const UART0_BASE = (char*)(0xFE201000);

// Manual switching between addresses required
```

## The Solution (After)

### 1. Include Configuration
```c
#include "mmio_config.h"
```

### 2. Use Configuration Constants
```c
static char* const UART0_BASE = (char*)(CONFIG_UART0_BASE);
static char* const UART3_BASE = (char*)(CONFIG_UART3_BASE);
static char* const GIC_BASE = (unsigned*)(CONFIG_GICD_BASE);
```

### 3. Build for Desired Platform
```bash
make PLATFORM=QEMU_VIRT       # → UART at 0x09000000
make PLATFORM=RPI4           # → UART at 0xFE201000
```

**That's it!** No code changes needed when switching platforms.

---

## Available Configuration Constants

### UART Addresses
- `CONFIG_UART0_BASE` - Primary UART
- `CONFIG_UART3_BASE` - Secondary UART

### GIC (Interrupt Controller)
- `CONFIG_GIC_BASE` - GIC base address
- `CONFIG_GICD_BASE` - Distributor
- `CONFIG_GICC_BASE` - CPU Interface

### Timers & GPIO
- `CONFIG_SYSTIMER_BASE` - System timer
- `CONFIG_STIMER_BASE` - Alternate system timer
- `CONFIG_GPIO_BASE` - GPIO controller
- `CONFIG_MMIO_BASE` - General MMIO base

### Register Offsets (Same for All Platforms)
- `UART_DR` (0x00) - Data register
- `UART_FR` (0x18) - Flag register
- `UART_CR` (0x30) - Control register
- `UART_LCRH` (0x2C) - Line control
- `UART_IBRD` (0x24) - Baud rate integer
- `UART_FBRD` (0x28) - Baud rate fractional

### Flag Bits
- `UART_FR_TXFF` - Transmit FIFO full
- `UART_CR_UARTEN` - UART enable (0x001)
- `UART_CR_TXE` - Transmit enable (0x100)
- `UART_CR_RXE` - Receive enable (0x200)
- `UART_LCRH_FEN` - FIFO enable (0x010)

### Helper Macros
```c
// Read/write single 32-bit register
MMIO_READ32(address)
MMIO_WRITE32(address, value)

// Read/write register at base + offset
MMIO_READ_REG(base, offset)
MMIO_WRITE_REG(base, offset, value)
```

---

## Complete Example

```c
#include "rpi.h"
#include "mmio_config.h"

void uart_init() {
    // Write control register to enable UART
    uint32_t cr = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
    MMIO_WRITE_REG(CONFIG_UART0_BASE, UART_CR, cr);
    
    // Read status
    uint32_t fr = MMIO_READ_REG(CONFIG_UART0_BASE, UART_FR);
    
    // Check if transmit FIFO full
    if (fr & UART_FR_TXFF) {
        // FIFO is full
    }
}

void send_char(char c) {
    // Write to data register
    MMIO_WRITE_REG(CONFIG_UART0_BASE, UART_DR, c);
}
```

Works on both QEMU and RPi4 without modification!

---

## Make Commands Cheatsheet

```bash
# Build for QEMU virt (default)
make
make PLATFORM=QEMU_VIRT

# Build for Raspberry Pi 4
make PLATFORM=RPI4

# Clean and rebuild for specific platform
make clean
make PLATFORM=RPI4

# Verify both platforms compile
./verify_builds.sh  # (if script provided)
```

---

## Adding Support for a New Platform

1. Edit [layer1-processes/mmio_config.h](layer1-processes/mmio_config.h)
2. Add under "RASPBERRY PI 4" section:
   ```c
   #elif TARGET_NEW_PLATFORM == 1
     #define CONFIG_UART0_BASE 0x...(new platform address)...
     #define CONFIG_GIC_BASE   0x...(new platform address)...
     // ... other addresses ...
   ```

3. Update [Makefile](Makefile) to recognize new platform:
   ```makefile
   else ifeq ($(PLATFORM),NEW_PLATFORM)
     PLATFORM_FLAG := -DTARGET_NEW_PLATFORM=1
   ```

4. Build: `make PLATFORM=NEW_PLATFORM`

That's all!

---

## Current Status

✅ **QEMU virt machine**
- Memory: 0x09000000 (UART), 0x08000000 (GIC)
- Architecture: ARM Cortex-A72
- Kernel load: 0x40000000

✅ **Raspberry Pi 4**
- Memory: 0xFE201000 (UART0), 0xFF840000 (GIC)
- Architecture: ARM Cortex-A72
- Kernel load: 0x40000000

✅ **Boot System**
- ✅ No code changes needed when switching
- ✅ Automatic platform detection at compile time
- ✅ Single source of truth for addresses
- ✅ Clean, maintainable design

---

## Files

- 📄 [mmio_config.h](layer1-processes/mmio_config.h) - Central configuration
- 📄 [rpi.c](layer1-processes/rpi.c) - UART/GPIO driver (uses CONFIG_*)
- 📄 [gic.c](layer1-processes/gic.c) - Interrupt driver (uses CONFIG_*)
- 📄 [systimer.c](layer1-processes/systimer.c) - Timer driver (uses CONFIG_*)
- 📄 [Makefile](Makefile) - Build system with platform selection
- 📖 [MMIO_CONFIG_README.md](MMIO_CONFIG_README.md) - Full documentation
- 📊 [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) - Technical details

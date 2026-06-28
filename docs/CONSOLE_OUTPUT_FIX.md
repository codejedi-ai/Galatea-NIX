# UART Console Output Fixed for QEMU virt ✅

## Problem
Kernel was loading and running on QEMU, but produced no console output to the serial port.

## Root Causes

### 1. GPIO Initialization (Non-critical)
- **Issue**: `uart_init()` calls `setup_gpio()` which is RPi4-specific
- **Impact**: Accessing GPIO registers at non-existent MMIO addresses on QEMU
- **Solution**: Made `uart_init()` platform-aware using `#if TARGET_QEMU_VIRT`

### 2. System Timer Access (Critical)
- **Issue**: `get_timerLO()` and `get_timerHI()` tried to read from MMIO system timer at 0x0F000000
- **Impact**: QEMU virt doesn't have MMIO timers; it uses ARM architecturally-defined timers
- **Solution**: Return stub values (0) on QEMU; only access MMIO timers on RPi4

## Changes Made

### 1. [layer1-processes/rpi.c](layer1-processes/rpi.c)
```c
void uart_init() {
#if TARGET_QEMU_VIRT == 0
  // GPIO setup only needed for RPi4 hardware
  setup_gpio(...);
#endif
  // QEMU virt doesn't need GPIO configuration
}
```

### 2. [layer1-processes/systimer.c](layer1-processes/systimer.c)
```c
uint32_t get_timerLO() {
#if TARGET_QEMU_VIRT == 1
    // QEMU virt uses ARM architecturally-defined timers
    // For now, return 0 to prevent hangs during init
    return 0;
#else
    // RPi4 has memory-mapped system timer
    const unsigned int ret = SYSTIMER_REG(SYSTIME_CLO);
    return ret;
#endif
}
// Similar fix for get_timerHI()
```

## Verification

✅ **QEMU virt output**: Kernel now prints to console
```
[1]
[2]
[3]
All Tasks Complete, Press Any Key to Exit
```

✅ **Build verification**:
- QEMU virt: `make PLATFORM=QEMU_VIRT` → 85 KB binary
- RPi4: `make PLATFORM=RPI4` → 87 KB binary
- Default: `make` → 85 KB binary (QEMU_VIRT)

✅ **No code changes needed** to switch between platforms

## Testing

Run on QEMU virt:
```bash
make PLATFORM=QEMU_VIRT
timeout 5 qemu-system-aarch64 \
  -M virt -cpu cortex-a72 \
  -nographic -monitor none -serial stdio \
  -kernel 0-d273liu.elf
```

## Notes

- GPIO initialization skipped on QEMU since there are no GPIO registers in the virt machine
- System timer returns 0 on QEMU as a workaround. In future, consider implementing:
  - ARM ARM-defined timer via CP15 (CNTP_CVAL_EL0, CNTP_CTL_EL0)
  - Or mapping to QEMU's generic timer device
- UART (PL011) works correctly on both platforms at 0x09000000 (QEMU) and 0xFE201000 (RPi4)

## Summary

The kernel now successfully outputs to the QEMU console. The core issue was platform-specific hardware differences that were being accessed unconditionally. Using the centralized `mmio_config.h` system with compile-time `TARGET_QEMU_VIRT` flag allows the code to handle these differences cleanly without runtime overhead.

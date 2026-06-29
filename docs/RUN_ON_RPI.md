# Running d273liu-nix on Raspberry Pi 4

**Pi 4 / BCM2711 only** (`start4.elf`, `bcm2711-rpi-4-b.dtb`). Pi 3 and earlier
are not supported.

The Pi 4 firmware loads a raw `kernel8.img` at **0x80000**. Docker uses QEMU
**`-M raspi4b`** with the same link address and MMIO map.

## Build the artifacts

```bash
./dev.sh pi          # builds ./pi-boot/kernel8.img + config.txt
```

(Or `make` → `0-d273liu.img`, copy to `kernel8.img`.)

## Test in QEMU (Docker)

```bash
./dev.sh test        # QEMU raspi4b, same binary as real hardware
./dev.sh run         # interactive shell
```

Requires the dev image with QEMU 9+ (`./dev.sh rebuild-image` once).

## Prepare the SD card

The Pi 4 still needs the official GPU firmware on a **FAT32 boot partition**.
Easiest: start from a Raspberry Pi OS card (or download `raspberrypi/firmware`
`boot/` files), then drop our files in.

On the FAT32 boot partition you need:

| File | Source |
|---|---|
| `start4.elf`, `fixup4.dat` | official Pi firmware (`raspberrypi/firmware/boot/`) |
| `bcm2711-rpi-4-b.dtb` | official Pi firmware |
| `config.txt` | **from `pi-boot/`** (overwrites the stock one) |
| `kernel8.img` | **from `pi-boot/`** (our kernel) |

Remove any `cmdline.txt`/extra kernels to avoid confusion.

## Serial console (how you see output)

d273liu-nix talks over the **PL011 UART (UART0 / ttyAMA0)** at 115200 8N1. Our
`config.txt` sets `enable_uart=1` + `dtoverlay=disable-bt` to route PL011 to
the GPIO header:

- GPIO 14 (pin 8)  = TX  → USB-serial RX
- GPIO 15 (pin 10) = RX  → USB-serial TX
- GND  (pin 6)     = GND

Connect a 3.3 V USB-serial adapter and open it at 115200:
`screen /dev/tty.usbserial-XXXX 115200` (mac) / `picocom -b 115200 /dev/ttyUSB0`.

Power the Pi; you should see the `[====] d273liu-nix Kernel Boot` banner.

## Honest status / what to expect

Likely points to iterate on real hardware:

1. **UART** — first thing to confirm on the Pi. QEMU raspi4b should match.
2. **GIC / timer interrupt** — `CLOCKINTID` (99) may need Pi-specific SPI/PPI
   numbers; cooperative tests may pass before timer IRQ is fully tuned.
3. **MMU on real Pi** — enabled in QEMU; verify the same page tables on hardware.

The realistic first milestone on real hardware is **the boot banner over serial**.

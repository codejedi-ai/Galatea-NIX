#!/usr/bin/env bash
# Build the Raspberry Pi 4 boot artifacts: kernel8.img + config.txt, staged in
# ./pi-boot/. Runs inside the dev container. Copy these onto the FAT boot
# partition of a Pi SD card alongside the official Pi firmware (see
# docs/RUN_ON_RPI.md). The same kernel is tested in Docker under QEMU raspi4b.
set -eu

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=source_project.sh
source "$ROOT/source_project.sh"

OUT=pi-boot
mkdir -p "$OUT"

echo "[*] Building kernel for Raspberry Pi 4 (link @ 0x80000, MMU off)..."
make clean >/dev/null 2>&1
make >/dev/null 2>&1

cp "$KERNEL_IMG" "$OUT/kernel8.img"

cat > "$OUT/config.txt" <<CFG
# ${PROJECT_NAME} on Raspberry Pi 4 (BCM2711), 64-bit bare metal.
arm_64bit=1
kernel=kernel8.img
# Route the PL011 (UART0, ttyAMA0) to GPIO 14/15 for the serial console.
enable_uart=1
dtoverlay=disable-bt
# Keep the GPU from changing the core clock under us.
core_freq=250
CFG

echo "[*] Done. Staged in ./$OUT :"
ls -la "$OUT"
echo
echo "    kernel8.img link/load address:"
aarch64-none-elf-readelf -l "$KERNEL_ELF" 2>/dev/null | grep -m1 LOAD | awk '{print "      "$3}'
echo "    See docs/RUN_ON_RPI.md for SD-card setup."

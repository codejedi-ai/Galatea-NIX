#!/usr/bin/env bash
# Build and boot the kernel interactively on QEMU raspi4b (Pi 4 hardware model).
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=source_project.sh
source "$ROOT/source_project.sh"

make || { echo "[!] build failed"; exit 1; }

echo "========================================="
echo "[*] Booting $KERNEL_IMG on QEMU raspi4b -- interactive terminal"
echo "[*] Quit QEMU with: Ctrl-A then X"
echo "========================================="

exec bash qemu-rpi4.sh "$KERNEL_IMG"

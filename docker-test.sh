#!/usr/bin/env bash
# Build and boot the kernel under QEMU's raspi4b machine (Pi 4 hardware model).
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=source_project.sh
source "$ROOT/source_project.sh"

TIMEOUT="${QEMU_TIMEOUT:-45}"

make || { echo "[!] build failed"; exit 1; }

echo "========================================="
echo "[*] Booting $KERNEL_IMG on QEMU raspi4b (load @ 0x80000, timeout ${TIMEOUT}s)"
echo "========================================="

timeout --foreground -s INT "${TIMEOUT}" \
  bash qemu-rpi4.sh "$KERNEL_IMG"

status=$?
echo ""
echo "========================================="
if [ "$status" -eq 124 ] || [ "$status" -eq 130 ]; then
  echo "[*] QEMU stopped after ${TIMEOUT}s timeout (kernel was idling) — OK"
else
  echo "[*] QEMU exited with status $status"
fi
echo "========================================="

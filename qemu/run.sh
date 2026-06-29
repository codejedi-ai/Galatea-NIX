#!/usr/bin/env bash
# qemu/run.sh — launch QEMU raspi4b.
#
# Serial wiring (MARKLIN_HW_UART3=0 build, standard QEMU):
#   serial0 (PL011 UART0   @ 0xFE201000) : OS console
#   serial1 (AUX mini-UART @ 0xFE215000) : Marklin virtual hardware (TCP)
#
# For real Pi (MARKLIN_HW_UART3=1) only serial0 is used from QEMU;
# UART3 is accessed directly on hardware and needs no -serial flag.
#
# Environment knobs:
#   CONSOLE=stdio|tcp|pty   — serial0 backend     (default: stdio)
#   MARKLIN=mhw|pty|off     — serial1/AUX backend  (default: mhw)
#   CONSOLE_PORT=7000       — TCP port when CONSOLE=tcp
#   QEMU_CPU=cortex-a72
#   QEMU_MEM=2048
set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# shellcheck source=../source_project.sh
source "$ROOT/source_project.sh"

IMG="${1:-$KERNEL_IMG}"
CPU="${QEMU_CPU:-cortex-a72}"
MEM="${QEMU_MEM:-2048}"
CONSOLE="${CONSOLE:-stdio}"
MARKLIN="${MARKLIN:-mhw}"
DTB="$ROOT/qemu/bcm2711-rpi-4-b.dtb"
MHW="$ROOT/marklin-virtual-hardware/my_program"

# stdio is a single resource: only one serial can claim it at a time.
if [ "$MARKLIN" = stdio ] && [ "$CONSOLE" = stdio ]; then CONSOLE=pty; fi

# ---- serial0: console (UART0) ------------------------------------------------
CONSOLE_SERIAL="stdio"
case "$CONSOLE" in
stdio) CONSOLE_SERIAL="stdio" ;;
pty)
  CONSOLE_SERIAL="pty"
  echo "[*] OS console -> pty (see 'char device redirected' below)"
  ;;
tcp)
  CONSOLE_PORT="${CONSOLE_PORT:-7000}"
  CONSOLE_SERIAL="tcp:0.0.0.0:${CONSOLE_PORT},server=on,wait=on"
  echo "[*] OS console -> TCP :$CONSOLE_PORT"
  ;;
*) echo "[!] unknown CONSOLE='$CONSOLE' (use stdio|tcp|pty); using stdio" >&2 ;;
esac

# ---- serial1: AUX mini-UART — Marklin virtual hardware -----------------------
# Build marklin-virtual-hardware, cleaning first if the binary isn't a Linux ELF.
if [ -d "$ROOT/marklin-virtual-hardware" ]; then
  _mhw_magic=$(od -An -N4 -tx1 "$MHW" 2>/dev/null | tr -d ' \n')
  if [ -f "$MHW" ] && [ "$_mhw_magic" != "7f454c46" ]; then
    echo "[*] Cleaning non-Linux marklin-virtual-hardware build..."
    (cd "$ROOT/marklin-virtual-hardware" && make clean -s) || true
  fi
  (cd "$ROOT/marklin-virtual-hardware" && make -s) || true
fi

MARKLIN_SERIAL="null"
case "$MARKLIN" in
vhw)
  # LINK_PORT set by the bridge; QEMU is the TCP server, vhw.py connects as client.
  LINK_PORT="${LINK_PORT:-6011}"
  MARKLIN_SERIAL="tcp:127.0.0.1:${LINK_PORT},server=on,wait=off"
  echo "[*] Marklin AUX (serial1) -> vhw.py on :$LINK_PORT (QEMU server)"
  ;;
pty)
  MARKLIN_SERIAL="pty"
  echo "[*] Marklin AUX (serial1) -> pty"
  ;;
stdio)
  MARKLIN_SERIAL="stdio"
  echo "[*] Marklin AUX (serial1) -> THIS terminal"
  ;;
off)
  echo "[*] Marklin AUX (serial1) = null"
  ;;
*) echo "[!] unknown MARKLIN='$MARKLIN' (use mhw|pty|stdio|off); AUX = null" >&2 ;;
esac

# ---- QEMU launch -------------------------------------------------------------
exec qemu-system-aarch64 \
  -M raspi4b \
  -cpu "$CPU" \
  -smp 4 \
  -m "$MEM" \
  -accel tcg,thread=multi \
  -dtb "$DTB" \
  -display none \
  -serial "$CONSOLE_SERIAL" \
  -serial "$MARKLIN_SERIAL" \
  -nic none \
  -kernel "$IMG"

#!/usr/bin/env bash
# One ttyd client -> QEMU console on stdio (direct PTY, no TCP/socat relay).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=source_project.sh
source "$ROOT/source_project.sh"

IMG="${KERNEL_IMG_NAME:-$KERNEL_IMG}"
LINK_PORT="$(python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1]);s.close()')"

export MARKLIN=vhw LINK_PORT="$LINK_PORT" CONSOLE=stdio
export DISK_ROOT="${DISK_ROOT:-/disk}"

cleanup() {
	kill -TERM -"$VHW_PID" 2>/dev/null || kill "$VHW_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

setsid python3 "$ROOT/tools/vhw.py" --port "$LINK_PORT" </dev/null &
VHW_PID=$!

bash "$ROOT/qemu-rpi4.sh" "$IMG"

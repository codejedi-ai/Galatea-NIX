#!/usr/bin/env bash
# Serve the OS console over the web with ttyd (default port 7681).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SESSION="$ROOT/display-screen/bridge/os-tty-session.sh"

cd "$ROOT"
exec ttyd -i 0.0.0.0 -W -t disableLeaveAlert=true bash "$SESSION"

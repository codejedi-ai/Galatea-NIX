#!/usr/bin/env bash
# Backward-compatibility wrapper — delegates to qemu/run.sh.
# All QEMU logic and documentation live in qemu/run.sh.
exec "$(dirname "$(realpath "${BASH_SOURCE[0]}")")/qemu/run.sh" "$@"

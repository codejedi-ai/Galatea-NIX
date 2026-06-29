#!/usr/bin/env bash
set -euo pipefail

cd /workspace

run_os() {
	echo "Starting GalateaOS under QEMU (virt). Ctrl+C to stop."
	make -j"$(nproc)" all
	exec qemu-system-aarch64 -machine virt -cpu cortex-a72 -m 1G -nographic \
		-kernel 0-d273liu.img
}

case "${1:-run}" in
	run)
		run_os
		;;
	shell|bash)
		exec bash
		;;
	build)
		make -j"$(nproc)" all
		;;
	make)
		shift
		exec make -j"$(nproc)" "$@"
		;;
	*)
		exec "$@"
		;;
esac

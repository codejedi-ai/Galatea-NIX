#!/usr/bin/env bash
# Dev wrapper — uses CS452ROTOS-PLATFORM image from Docker Hub (or local build).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLATFORM="$(cd "${ROOT}/../github_codejedi-ai_CS452ROTOS-PLATFORM" && pwd)"
IMAGE="${DARCYOS_IMAGE:-codejedi-ai/cs452rotos-platform:latest}"

usage() {
	cat <<'EOF'
Usage: ./dev.sh <command>

Commands:
  build-image Pull/build the shared platform image
  run         Build kernel and run layer tests under QEMU
  shell       Open a shell in the dev container
  make [args] Run make inside the container

Environment:
  DARCYOS_IMAGE   default: codejedi-ai/cs452rotos-platform:latest
EOF
}

ensure_image() {
	DARCYOS_IMAGE="${IMAGE}"
	# shellcheck source=/dev/null
	source "${PLATFORM}/scripts/ensure-image.sh"
}

docker_run() {
	ensure_image
	local -a args=(--rm -v "${ROOT}:/workspace" -w /workspace -e XDIR=/opt/toolchain -e IN_DOCKER=1)
	if [ -t 0 ] && [ -t 1 ]; then
		args+=(-it)
	else
		args+=(-i)
	fi
	if [ -e /dev/kvm ]; then
		args+=(--device /dev/kvm)
		if command -v getent >/dev/null 2>&1 && getent group kvm >/dev/null 2>&1; then
			args+=(--group-add "$(getent group kvm | cut -d: -f3)")
		fi
	fi
	args+=(--entrypoint bash "${IMAGE}")
	docker run "${args[@]}" -lc 'exec /workspace/scripts/container-run.sh "$@"' -- "$@"
}

cmd="${1:-run}"
shift || true

case "${cmd}" in
	build|build-image)
		ensure_image
		;;
	run)
		docker_run run
		;;
	shell|bash)
		docker_run shell
		;;
	make)
		docker_run make -j"$(nproc)" "$@"
		;;
	help|-h|--help)
		usage
		;;
	*)
		echo "Unknown command: ${cmd}" >&2
		usage >&2
		exit 1
		;;
esac

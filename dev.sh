#!/usr/bin/env bash
#
# Dev-environment driver — uses CS452ROTOS-PLATFORM (codejedi-ai/cs452rotos-platform:latest).
# NIX repos also expose docker compose services (screen, prod) via compose directly.
#
#   ./dev.sh shell          # interactive shell in the container (source at /src)
#   ./dev.sh make [args]    # run `make [args]` inside the container (same as build)
#   ./dev.sh build [args]   # run `make [args]` inside the container
#   ./dev.sh build-image    # pull/ensure the shared platform image
#   ./dev.sh run            # build + boot the kernel INTERACTIVELY (the OS terminal)
#   ./dev.sh test           # build + boot on QEMU raspi4b (timed smoke test)
#   ./dev.sh clean          # run `make clean` inside the container
#   ./dev.sh pi             # build Raspberry Pi 4 artifacts (kernel8.img)
#   ./dev.sh link-test      # boot + assert the OS reaches the right Marklin mock
#
# CPU cap is DEV_CPUS (default 1) in docker-compose.yml; override per run, e.g.
#   DEV_CPUS=4 ./dev.sh make all
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLATFORM="$(cd "${ROOT}/../github_codejedi-ai_CS452ROTOS-PLATFORM" && pwd)"
cd "$ROOT"

DC=(docker compose)

ensure_image() {
	DARCYOS_IMAGE="${DARCYOS_IMAGE:-codejedi-ai/cs452rotos-platform:latest}"
	# shellcheck source=/dev/null
	source "${PLATFORM}/scripts/ensure-image.sh"
}

cmd="${1:-shell}"
shift || true

case "$cmd" in
  shell)         ensure_image; "${DC[@]}" run --rm shell ;;
  build)         ensure_image; "${DC[@]}" run --rm -T shell make "$@" ;;
  make)          ensure_image; "${DC[@]}" run --rm -T shell make "$@" ;;
  build-image)   ensure_image ;;
  run)           ensure_image; "${DC[@]}" run --rm run ;;
  test)          ensure_image; "${DC[@]}" run --rm -T test ;;
  clean)         ensure_image; "${DC[@]}" run --rm -T shell make clean ;;
  pi)            ensure_image; "${DC[@]}" run --rm -T shell bash mkpi.sh ;;
  link-test)     ensure_image; "${DC[@]}" run --rm -T shell bash tools/test_link.sh ;;
  rebuild-image) ensure_image ;;
  *)
    echo "usage: $0 {shell|make [args]|build [args]|build-image|run|test|clean|pi|link-test|rebuild-image}" >&2
    exit 1 ;;
esac

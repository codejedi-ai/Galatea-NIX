#!/usr/bin/env bash
# Production driver for the browser screen — a thin wrapper around `docker compose`.
# The `prod` service in docker-compose.yml owns the image, CPU cap, and port.
#
#   ./prod.sh up       build + run ttyd web terminal; http://localhost:7681
#   ./prod.sh build    just (re)build the prod image
#   ./prod.sh down     stop the browser screen
#   ./prod.sh logs     follow the container logs
#
# Override the published port with PROD_PORT or the CPU cap with PROD_CPUS, e.g.:
#   PROD_PORT=8080 ./prod.sh up
#
# Equivalent raw compose commands also work, e.g. `docker compose up -d prod`.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
# shellcheck source=source_project.sh
source "$ROOT/source_project.sh"

DC=(docker compose)
PORT="${PROD_PORT:-7681}"

# Dockerfile.prod is FROM the dev image; make sure it exists before building prod.
ensure_dev() {
  docker image inspect "$DOCKER_DEV_IMG" >/dev/null 2>&1 || "${DC[@]}" build shell
}

case "${1:-up}" in
  up)
    ensure_dev
    "${DC[@]}" up -d --build prod
    echo
    echo "==================================================="
    echo "  ${PROJECT_NAME} ttyd screen is UP"
    echo "  ->  http://localhost:${PORT}"
    echo "==================================================="
    echo "  stop with: ./prod.sh down" ;;
  build) ensure_dev; "${DC[@]}" build prod ;;
  down)  "${DC[@]}" down ;;
  logs)  "${DC[@]}" logs -f prod ;;
  *)     echo "usage: $0 {up|build|down|logs}" >&2; exit 1 ;;
esac

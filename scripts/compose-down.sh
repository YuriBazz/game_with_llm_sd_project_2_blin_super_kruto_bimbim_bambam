#!/usr/bin/env bash
# Stop and remove all compose containers
set -euo pipefail
cd "$(dirname "$0")/.."
docker compose down --remove-orphans "$@"
# Optional: remove dangling <none> images left after rebuilds
# docker image prune -f

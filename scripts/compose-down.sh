#!/usr/bin/env bash
# Stop and remove all compose containers (including profiled ollama/cursor services)
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ -f .env ]]; then
  set -a
  # shellcheck disable=SC1091
  source .env
  set +a
fi

profiles=(--profile ollama --profile cursor-h --profile cursor-a)
docker compose "${profiles[@]}" down --remove-orphans "$@"

#!/usr/bin/env bash
# Build (all project images) + start full stack: game-service, mcp-server,
# agent-runner, web-client, ollama. Loads .env from repo root.
set -euo pipefail
cd "$(dirname "$0")/.."

usage() {
  cat <<'EOF'
Usage: ./scripts/compose-up.sh [options] [docker compose up args...]

One command to build and run the full stack.

Options:
  --godmode       Human spectator (noclip, enemies ignore you, full map)
  --log=PATH      Write compose output to PATH (truncates existing file)
  --no-build      Skip image build (docker compose up only)
  --no-cache      Rebuild all images from scratch (docker compose build --no-cache)
  --build         Accepted for compatibility; build is ON by default
  -h, --help      Show this help

Examples:
  ./scripts/compose-up.sh --godmode --log=./logs/logs.log
  ./scripts/compose-up.sh --no-cache --godmode
  ./scripts/compose-up.sh --no-build

Web UI: http://localhost:5173 — press Start Game
EOF
}

if [[ -f .env ]]; then
  set -a
  # shellcheck disable=SC1091
  source .env
  set +a
fi

export GODMODE="${GODMODE:-false}"
LOG_FILE=""
NO_CACHE=false
NO_BUILD=false
compose_args=()

for arg in "$@"; do
  case "$arg" in
    --godmode)
      export GODMODE=true
      ;;
    --log=*)
      LOG_FILE="${arg#*=}"
      ;;
    --log)
      echo "Error: use --log=path (e.g. --log=./logs/logs.log)" >&2
      exit 1
      ;;
    --no-cache)
      NO_CACHE=true
      ;;
    --no-build)
      NO_BUILD=true
      ;;
    --build)
      ;; # default: build everything before up
    -h|--help)
      usage
      exit 0
      ;;
    *)
      compose_args+=("$arg")
      ;;
  esac
done

if [[ "${GODMODE}" == "true" || "${GODMODE}" == "1" || "${GODMODE}" == "yes" ]]; then
  echo "GODMODE enabled — human noclip spectator, enemies ignore you"
fi

run_up() {
  local -a cmd=(docker compose up)
  if ((${#compose_args[@]} > 0)); then
    cmd+=("${compose_args[@]}")
  fi
  if [[ -n "$LOG_FILE" ]]; then
    local log_dir
    log_dir=$(dirname "$LOG_FILE")
    if [[ "$log_dir" != "." && -n "$log_dir" ]]; then
      mkdir -p "$log_dir"
    fi
    if [[ -f "$LOG_FILE" ]]; then
      echo "Clearing existing log file: $LOG_FILE"
      : > "$LOG_FILE"
    fi
    echo "Logging compose output to: $LOG_FILE"
    "${cmd[@]}" 2>&1 | tee -a "$LOG_FILE"
    return "${PIPESTATUS[0]}"
  fi
  exec "${cmd[@]}"
}

if [[ "$NO_BUILD" == true ]]; then
  echo "Skipping build (--no-build)"
  run_up
  exit $?
fi

echo "=== Building all project images ==="
echo "  game-service, mcp-server, agent-runner, web-client"
build_cmd=(docker compose build)
if [[ "$NO_CACHE" == true ]]; then
  echo "  (--no-cache: full rebuild)"
  build_cmd+=(--no-cache)
fi
"${build_cmd[@]}"

echo "=== Starting full stack ==="
echo "  game-service :8080, web-client :5173, ollama :11434, agent-runner, mcp-server"
echo "  LLM_PROVIDER=${LLM_PROVIDER:-mock}  OLLAMA_MODEL=${OLLAMA_MODEL:-qwen2.5:3b}"
if [[ -e /dev/dri/renderD128 ]]; then
  echo "  GPU: AMD (/dev/dri) — Ollama uses Vulkan"
elif command -v nvidia-smi >/dev/null 2>&1; then
  echo "  GPU: NVIDIA detected — use: docker compose -f docker-compose.yml -f docker-compose.nvidia.yml up"
fi
run_up

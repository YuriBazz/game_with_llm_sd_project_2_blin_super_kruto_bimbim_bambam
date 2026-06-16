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
  --godmode       Enable spectator camera (default in compose: GODMODE=true)
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

export GODMODE="${GODMODE:-true}"
LOG_FILE=""
NO_CACHE=false
NO_BUILD=false
compose_args=()
compose_profiles=()

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

trim_env() {
  printf '%s' "${1:-}" | tr -d '[:space:]'
}

robot_llm_provider_for_label() {
  case "$1" in
    H) printf '%s' "${ROBOT_H_LLM_PROVIDER:-${ROBOT_H_LLM_PROVIDER_FALLBACK:-ollama}}" ;;
    A) printf '%s' "${ROBOT_A_LLM_PROVIDER:-${ROBOT_A_LLM_PROVIDER_FALLBACK:-ollama}}" ;;
    *) printf '%s' "ollama" ;;
  esac
}

maybe_enable_ollama() {
  local provider_a provider_h
  provider_a="$(robot_llm_provider_for_label A)"
  provider_h="$(robot_llm_provider_for_label H)"

  if [[ "$provider_a" == "ollama" || "$provider_h" == "ollama" ]]; then
    compose_profiles+=("ollama")
    echo "  ollama: enabled"
    if [[ "$provider_a" == "ollama" ]]; then
      echo "    robot A: pull ${ROBOT_A_MODEL:-${ROBOT_A_MODEL_FALLBACK:-(unset)}}"
    else
      echo "    robot A: skip (provider=${provider_a})"
    fi
    if [[ "$provider_h" == "ollama" ]]; then
      echo "    robot H: pull ${ROBOT_H_MODEL:-${ROBOT_H_MODEL_FALLBACK:-(unset)}}"
    else
      echo "    robot H: skip (provider=${provider_h})"
    fi
    return 0
  fi

  echo "  ollama: skipped (no robot uses ollama)"
}

maybe_enable_cursor_bridge() {
  local label="$1"
  local profile="$2"
  local provider
  local key
  provider="$(robot_llm_provider_for_label "$label")"
  key="$(trim_env "${CURSOR_API_KEY:-}")"

  if [[ "$provider" != "cursor" ]]; then
    echo "  cursor-llm-bridge-${label,,}: skipped (ROBOT_${label}_LLM_PROVIDER=${provider})"
    return 0
  fi

  if [[ -z "$key" ]]; then
    echo "Error: ROBOT_${label}_LLM_PROVIDER=cursor but CURSOR_API_KEY is missing or empty." >&2
    exit 1
  fi

  compose_profiles+=("$profile")
  echo "  cursor-llm-bridge-${label,,}: enabled"
}

maybe_enable_ollama
maybe_enable_cursor_bridge H cursor-h
maybe_enable_cursor_bridge A cursor-a

if [[ "${GODMODE}" == "true" || "${GODMODE}" == "1" || "${GODMODE}" == "yes" ]]; then
  echo "GODMODE enabled — spectator fly camera; robots H+A controlled by agents"
fi

append_compose_profiles() {
  local -n cmd_ref=$1
  local profile
  for profile in "${compose_profiles[@]}"; do
    cmd_ref+=(--profile "$profile")
  done
}

run_up() {
  local -a cmd=(docker compose)
  append_compose_profiles cmd
  cmd+=(up)
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
build_cmd=(docker compose)
append_compose_profiles build_cmd
build_cmd+=(build)
if [[ "$NO_CACHE" == true ]]; then
  echo "  (--no-cache: full rebuild)"
  build_cmd+=(--no-cache)
fi
"${build_cmd[@]}"

echo "=== Starting full stack ==="
echo "  game-service :8080, web-client :5173, agent-runner-player, agent-runner-rival"
echo "  Robot A: $(robot_llm_provider_for_label A) model=${ROBOT_A_MODEL:-${ROBOT_A_MODEL_FALLBACK:-}}"
echo "  Robot H: $(robot_llm_provider_for_label H) model=${ROBOT_H_MODEL:-${ROBOT_H_MODEL_FALLBACK:-}}"
if [[ -e /dev/dri/renderD128 ]]; then
  echo "  GPU: AMD (/dev/dri) — Ollama uses Vulkan"
elif command -v nvidia-smi >/dev/null 2>&1; then
  echo "  GPU: NVIDIA detected — use: docker compose -f docker-compose.yml -f docker-compose.nvidia.yml up"
fi
run_up

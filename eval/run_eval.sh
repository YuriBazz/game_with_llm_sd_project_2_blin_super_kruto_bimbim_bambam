#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

RUNS="${1:-5}"
OUT_DIR="$ROOT/eval/logs"
mkdir -p "$OUT_DIR"

AGENTS=("mock:rival" "mock:player")
if [[ "${2:-}" == "--both" ]]; then
  AGENTS=("mock:rival" "mock:player" "ollama:rival" "ollama:player")
fi

echo "Running eval: runs=$RUNS agents=${AGENTS[*]}"

declare -a seeds=(11 22 33 44 55)

results_file="$ROOT/eval/results.md"
{
  echo "# Eval Results"
  echo
  echo "Generated: $(date -u +"%Y-%m-%dT%H:%M:%SZ")"
  echo
  echo "| Agent | Slot | Runs | Success Rate | Avg Steps | Avg Tokens | Avg Final HP |"
  echo "|-------|------|------|--------------|-----------|------------|--------------|"
} > "$results_file"

for spec in "${AGENTS[@]}"; do
  agent="${spec%%:*}"
  slot="${spec##*:}"
  success=0
  total_steps=0
  total_tokens=0
  total_hp=0

  for i in $(seq 1 "$RUNS"); do
    seed="${seeds[$((i - 1))]}"
    log_file="$OUT_DIR/${agent}_${slot}_seed_${seed}.log"

    echo "=== $agent/$slot run $i seed=$seed ==="
    env_args=()
    if [[ "$slot" == "player" ]]; then
      env_args+=(ROBOT_H_LLM_PROVIDER="$agent")
    else
      env_args+=(ROBOT_A_LLM_PROVIDER="$agent")
    fi

    if AGENT_SLOT="$slot" "${env_args[@]}" GAME_SEED="$seed" MAX_STEPS=80 \
      docker compose run --rm --no-deps "agent-runner-${slot}" > "$log_file" 2>&1; then
      if grep -qE "Round finished|Level complete" "$log_file"; then
        success=$((success + 1))
      fi
    fi

    steps=$(grep -c "=== Step" "$log_file" || true)
    tokens=$(grep "Tokens used:" "$log_file" | tail -1 | awk '{print $NF}' || echo 0)
    hp=$(grep -E "Robot (H|A) HP:" "$log_file" | tail -1 | grep -oE 'HP: [0-9]+' | awk '{print $2}' || echo 0)
    total_steps=$((total_steps + steps))
    total_tokens=$((total_tokens + tokens))
    total_hp=$((total_hp + hp))
  done

  success_rate=$(python3 - <<PY
print(round(100 * $success / $RUNS, 1))
PY
)
  avg_steps=$(python3 - <<PY
print(round($total_steps / $RUNS, 1))
PY
)
  avg_tokens=$(python3 - <<PY
print(round($total_tokens / $RUNS, 1))
PY
)
  avg_hp=$(python3 - <<PY
print(round($total_hp / $RUNS, 1))
PY
)

  echo "| $agent | $slot | $RUNS | ${success_rate}% | $avg_steps | $avg_tokens | $avg_hp |" >> "$results_file"
done

{
  echo
  echo "## Notes"
  echo "- Dual agents: \`agent-runner-player\` (H, slot=player) vs \`agent-runner-rival\` (A, slot=rival)."
  echo "- Seeds: ${seeds[*]}"
  echo "- Logs: eval/logs/"
} >> "$results_file"

echo "Wrote eval/results.md"
cat "$results_file"

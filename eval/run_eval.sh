#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

AGENT="${1:-mock}"
RUNS="${2:-5}"
OUT_DIR="$ROOT/eval/logs"
mkdir -p "$OUT_DIR"

echo "Running eval: agent=$AGENT runs=$RUNS"

declare -a seeds=(11 22 33 44 55)
success=0
total_steps=0
total_tokens=0

for i in $(seq 1 "$RUNS"); do
  seed="${seeds[$((i - 1))]}"
  log_file="$OUT_DIR/${AGENT}_seed_${seed}.log"

  echo "=== Run $i seed=$seed ==="
  if LLM_PROVIDER="$AGENT" GAME_SEED="$seed" MAX_STEPS=80 \
    docker compose run --rm --no-deps agent-runner > "$log_file" 2>&1; then
    if grep -q "Game finished" "$log_file"; then
      success=$((success + 1))
    fi
  fi

  steps=$(grep -c "=== Step" "$log_file" || true)
  tokens=$(grep "Tokens used:" "$log_file" | tail -1 | awk '{print $NF}' || echo 0)
  total_steps=$((total_steps + steps))
  total_tokens=$((total_tokens + tokens))
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

cat > "$ROOT/eval/results.md" <<EOF
# Eval Results

Generated: $(date -u +"%Y-%m-%dT%H:%M:%SZ")

| Agent | Runs | Success Rate | Avg Steps | Avg Tokens |
|-------|------|--------------|-----------|------------|
| $AGENT | $RUNS | ${success_rate}% | $avg_steps | $avg_tokens |

## Notes
- Agent runner uses MCP stdio + LLM tool selection.
- Seeds: ${seeds[*]}
- Logs: eval/logs/
EOF

echo "Wrote eval/results.md"
cat "$ROOT/eval/results.md"

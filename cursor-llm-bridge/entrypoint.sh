#!/bin/sh
set -e

label=$(printf '%s' "${ROBOT_LABEL:-}" | tr '[:lower:]' '[:upper:]')
case "$label" in
  A|H) ;;
  *)
    echo "ERROR: ROBOT_LABEL must be A or H; cursor-llm-bridge will not start." >&2
    exit 1
    ;;
esac

key_trimmed=$(printf '%s' "${CURSOR_API_KEY:-}" | tr -d '[:space:]')
if [ -z "$key_trimmed" ]; then
  echo "ERROR: CURSOR_API_KEY is required; cursor-llm-bridge will not start." >&2
  exit 1
fi

exec uvicorn main:app --host 0.0.0.0 --port "${PORT:-8765}"

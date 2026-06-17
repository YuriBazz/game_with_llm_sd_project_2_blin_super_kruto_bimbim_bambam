#!/bin/sh
set -e

ollama serve &
sleep 5

if [ -n "${OLLAMA_PULL_MODEL:-}" ]; then
  echo "ollama: pull ${OLLAMA_PULL_MODEL}"
  ollama pull "${OLLAMA_PULL_MODEL}"
else
  echo "ollama: OLLAMA_PULL_MODEL is empty, skip pull"
fi

exec tail -f /dev/null

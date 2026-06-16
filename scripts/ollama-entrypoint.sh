#!/bin/sh
set -e

pull_for_robot() {
  label="$1"
  provider="$2"
  model="$3"

  if [ "$provider" != "ollama" ]; then
    echo "ollama: skip robot ${label} (provider=${provider})"
    return 0
  fi

  if [ -z "$model" ]; then
    echo "ollama: skip robot ${label} (ROBOT_${label}_MODEL is empty)" >&2
    return 0
  fi

  echo "ollama: pull robot ${label} model=${model}"
  ollama pull "$model"
}

ollama serve &
sleep 5

pull_for_robot A "${ROBOT_A_LLM_PROVIDER:-ollama}" "${ROBOT_A_MODEL:-}"
pull_for_robot H "${ROBOT_H_LLM_PROVIDER:-ollama}" "${ROBOT_H_MODEL:-}"

exec tail -f /dev/null

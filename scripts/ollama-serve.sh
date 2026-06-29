#!/bin/sh

ollama serve &
echo "ollama: server started (model pull on demand via agent-runner)"

exec tail -f /dev/null

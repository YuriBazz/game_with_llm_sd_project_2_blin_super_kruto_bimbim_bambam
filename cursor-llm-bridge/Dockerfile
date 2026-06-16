FROM python:3.12-slim

RUN apt-get update && apt-get install -y --no-install-recommends curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY cursor-llm-bridge/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY cursor-llm-bridge/main.py ./
COPY agent-runner/prompts/player_llm_prompt.txt /app/prompts/player_llm_prompt.txt

ENV CURSOR_CWD=/workspace
ENV PORT=8765
ENV LLM_PROMPT_PATH=/app/prompts/player_llm_prompt.txt
ENV HEALTH_PROBE_INTERVAL_SEC=90

EXPOSE 8765

HEALTHCHECK --interval=90s --timeout=30s --start-period=60s --retries=3 \
    CMD curl -fsS "http://127.0.0.1:${PORT}/health" | grep -q '"status":"ok"'

CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8765"]

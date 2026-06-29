FROM python:3.12-slim

RUN apt-get update && apt-get install -y --no-install-recommends curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY cursor-llm-bridge/requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY cursor-llm-bridge/main.py ./
COPY cursor-llm-bridge/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
COPY agent-runner/prompts/robot_llm_prompt.txt /app/prompts/robot_llm_prompt.txt
RUN sed -e 's/{slot}/player/g' -e 's/{marker}/H/g' -e 's/{other_slot}/rival/g' -e 's/{other_marker}/A/g' \
        /app/prompts/robot_llm_prompt.txt > /app/prompts/robot_H_llm_prompt.txt \
    && sed -e 's/{slot}/rival/g' -e 's/{marker}/A/g' -e 's/{other_slot}/player/g' -e 's/{other_marker}/H/g' \
        /app/prompts/robot_llm_prompt.txt > /app/prompts/robot_A_llm_prompt.txt

ENV ROBOT_H_CURSOR_CWD=/workspace
ENV ROBOT_A_CURSOR_CWD=/workspace
ENV PORT=8765

EXPOSE 8765

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -fsS "http://127.0.0.1:${PORT}/health" | grep -q '"status":"ok"'

ENTRYPOINT ["/entrypoint.sh"]

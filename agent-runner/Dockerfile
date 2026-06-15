FROM ubuntu:22.04 AS mcp-builder

RUN apt-get update && apt-get install -y \
    cmake build-essential git \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY mcp-server/CMakeLists.txt mcp-server/CMakeLists.txt
COPY mcp-server/src/ mcp-server/src/
RUN mkdir -p mcp-server/build && cd mcp-server/build && cmake .. && cmake --build . --target mcp_server

FROM ubuntu:22.04 AS agent-builder

RUN apt-get update && apt-get install -y \
    cmake build-essential git \
    libssl-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY agent-runner/CMakeLists.txt .
COPY agent-runner/src/ ./src/
RUN mkdir build && cd build && cmake .. && cmake --build .

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libcurl4 \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=mcp-builder /app/mcp-server/build/mcp_server /usr/local/bin/mcp_server
COPY --from=agent-builder /app/build/agent_runner /usr/local/bin/agent_runner

ENV MCP_SERVER_PATH=/usr/local/bin/mcp_server
ENV LLM_PROVIDER=mock
ENV MAX_STEPS=100
ENV GAME_SEED=42

CMD ["/usr/local/bin/agent_runner"]

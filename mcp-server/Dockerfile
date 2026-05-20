FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    cmake build-essential git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt .
COPY src/ ./src/

RUN mkdir build && cd build && \
    cmake .. && \
    cmake --build . --target mcp_server

CMD ["./build/mcp_server"]

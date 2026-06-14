FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    cmake build-essential git \
    libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt .
COPY src/ ./src/
COPY include/ ./include/

RUN mkdir build && cd build && \
    cmake .. && \
    cmake --build . --verbose

EXPOSE 8080
CMD ["./build/game_service"]

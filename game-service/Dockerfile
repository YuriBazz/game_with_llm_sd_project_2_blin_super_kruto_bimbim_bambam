FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    cmake build-essential \
    libssl-dev \
    curl \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt .
COPY third_party/ ./third_party/
COPY src/ ./src/
COPY include/ ./include/

RUN mkdir build && cd build && \
    cmake .. -DBUILD_TESTS=OFF && \
    cmake --build . -j"$(nproc)"

EXPOSE 8080
CMD ["./build/game_service"]

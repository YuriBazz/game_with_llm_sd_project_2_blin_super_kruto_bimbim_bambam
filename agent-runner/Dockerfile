FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    cmake build-essential git \
    libssl-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 --branch v0.18.0 https://github.com/yhirose/cpp-httplib.git /usr/local/include/httplib

WORKDIR /app

COPY CMakeLists.txt .
COPY src/ ./src/

RUN mkdir build && cd build && \
    cmake .. && \
    cmake --build .

CMD ["./build/agent_runner"]

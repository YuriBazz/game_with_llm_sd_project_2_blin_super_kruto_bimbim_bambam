cmake_minimum_required(VERSION 3.16)
project(mcp_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

add_executable(mcp_server src/main.cpp src/mcp_core.hpp)
target_include_directories(mcp_server PRIVATE ${json_SOURCE_DIR}/include)

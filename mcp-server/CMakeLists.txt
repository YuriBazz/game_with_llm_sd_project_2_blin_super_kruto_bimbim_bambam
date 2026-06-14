cmake_minimum_required(VERSION 3.16)
project(mcp_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find curl
find_package(CURL REQUIRED)

# Try to use json headers from ../build/_deps if available (local development)
# Otherwise fetch them during build (Docker)
set(JSON_INCLUDE_DIR "")
if(EXISTS "${CMAKE_SOURCE_DIR}/../build/_deps/json-src/include")
    set(JSON_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../build/_deps/json-src/include")
else()
    # Fallback: use FetchContent for Docker builds
    include(FetchContent)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(json)
    set(JSON_INCLUDE_DIR "${json_SOURCE_DIR}/include")
endif()

add_executable(mcp_server src/main.cpp src/mcp_core.hpp)
target_include_directories(mcp_server PRIVATE ${JSON_INCLUDE_DIR})
target_link_libraries(mcp_server PRIVATE CURL::libcurl)

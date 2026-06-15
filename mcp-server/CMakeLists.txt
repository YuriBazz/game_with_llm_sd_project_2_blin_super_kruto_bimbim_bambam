cmake_minimum_required(VERSION 3.16)
project(mcp_server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find curl
find_package(CURL REQUIRED)
find_package(nlohmann_json QUIET)

set(JSON_INCLUDE_DIR "")
if(nlohmann_json_FOUND)
    set(JSON_TARGET nlohmann_json::nlohmann_json)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../build/_deps/json-src/include")
    set(JSON_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../build/_deps/json-src/include")
else()
    include(FetchContent)
    FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(json)
    set(JSON_INCLUDE_DIR "${json_SOURCE_DIR}/include")
endif()

add_executable(mcp_server src/main.cpp src/mcp_core.hpp)
if(JSON_TARGET)
    target_link_libraries(mcp_server PRIVATE ${JSON_TARGET})
else()
    target_include_directories(mcp_server PRIVATE ${JSON_INCLUDE_DIR})
endif()
target_link_libraries(mcp_server PRIVATE CURL::libcurl)

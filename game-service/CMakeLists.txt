cmake_minimum_required(VERSION 3.16)
project(game_service)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# cpp-httplib
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.0
)
FetchContent_MakeAvailable(httplib)

# nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

add_executable(game_service src/main.cpp)

target_link_libraries(game_service PRIVATE httplib)
target_include_directories(game_service PRIVATE ${json_SOURCE_DIR}/include)

find_package(Threads REQUIRED)
target_link_libraries(game_service PRIVATE Threads::Threads)

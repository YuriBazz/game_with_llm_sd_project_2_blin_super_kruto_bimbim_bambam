cmake_minimum_required(VERSION 3.16)
project(agent_runner)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(agent_runner src/main.cpp src/llm_client.hpp)

target_link_libraries(agent_runner PRIVATE
    Threads::Threads
    ${CURL_LIBRARIES}
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(agent_runner PRIVATE
    ${CURL_INCLUDE_DIRS}
    /usr/include/nlohmann
)

cmake_minimum_required(VERSION 3.16)
project(game_service)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    )
    FetchContent_MakeAvailable(json)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/httplib/httplib.h")
    add_library(httplib INTERFACE)
    target_include_directories(httplib INTERFACE "${CMAKE_SOURCE_DIR}/third_party/httplib")
else()
    include(FetchContent)
    FetchContent_Declare(
        httplib
        URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.18.0.tar.gz
    )
    FetchContent_MakeAvailable(httplib)
endif()

add_executable(game_service
        src/main.cpp
        src/map/MapGenerator.cpp
        src/map/MapOptions.cpp
        src/map/Rect.cpp
        src/map/TileType.cpp
        src/map/LevelMap.cpp
        src/State.cpp
        src/RandomGenerator.cpp
        include/game_service/magic_enum.hpp
        include/concepts.hpp
)

target_link_libraries(game_service PRIVATE httplib)
if(nlohmann_json_FOUND)
    target_link_libraries(game_service PRIVATE nlohmann_json::nlohmann_json)
else()
    target_include_directories(game_service PRIVATE ${json_SOURCE_DIR}/include)
endif()
target_include_directories(game_service PRIVATE include)

find_package(Threads REQUIRED)
target_link_libraries(game_service PRIVATE Threads::Threads)

install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp"
)
install(EXPORT ${PROJECT_NAME}-targets
        FILE ${PROJECT_NAME}Config.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION lib/cmake/${PROJECT_NAME}
)

option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    add_executable(state_test tests/state_test.cpp src/State.cpp src/RandomGenerator.cpp
        src/map/MapGenerator.cpp src/map/MapOptions.cpp src/map/Rect.cpp
        src/map/TileType.cpp src/map/LevelMap.cpp)
    if(nlohmann_json_FOUND)
        target_link_libraries(state_test PRIVATE nlohmann_json::nlohmann_json)
    else()
        target_include_directories(state_test PRIVATE ${json_SOURCE_DIR}/include)
    endif()
    target_include_directories(state_test PRIVATE include)
    target_link_libraries(state_test PRIVATE httplib Threads::Threads)
    add_test(NAME state_test COMMAND state_test)
    enable_testing()
endif()

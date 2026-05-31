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

add_executable(game_service
        src/main.cpp
        include/game_service/map/MapGenerator.hpp
        src/map/MapGenerator.cpp
        include/game_service/map/MapOptions.hpp
        src/map/MapOptions.cpp
        include/game_service/map/Rect.hpp
        src/map/Rect.cpp
        include/game_service/map/TileType.hpp
        src/map/TileType.cpp
        include/game_service/map/LevelMap.hpp
        src/map/LevelMap.cpp
        include/game_service/entities/Enemy.hpp
        include/game_service/State.hpp
        include/game_service/entities/Player.hpp
        src/State.cpp
        include/game_service/RandomGenerator.hpp
        include/game_service/entities/Item.hpp
        include/game_service/entities/Loot.hpp
)

target_link_libraries(game_service PRIVATE httplib)
target_include_directories(game_service PRIVATE ${json_SOURCE_DIR}/include)
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

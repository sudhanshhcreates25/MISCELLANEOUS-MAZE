cmake_minimum_required(VERSION 3.16)
project(MiscellaneousMaze LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MAZE_ENGINE_SOURCES
    src/GameEngine.cpp
    src/GeneticProgramming.cpp
)

add_executable(maze_terminal
    src/Main.cpp
    ${MAZE_ENGINE_SOURCES}
)

find_package(raylib CONFIG REQUIRED
    HINTS /opt/homebrew/opt/raylib /usr/local/opt/raylib
)

add_executable(maze_graphical
    src/GraphicalMain.cpp
    ${MAZE_ENGINE_SOURCES}
)
target_link_libraries(maze_graphical PRIVATE raylib)

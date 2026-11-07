cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)
project(MiscellaneousMaze LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Match the static-CRT vcpkg triplet used by the Windows build. Keep the Debug
# runtime variant for Debug configurations; Release uses the non-debug CRT.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

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

if(WIN32)
    # The vcpkg Raylib port builds with external GLFW. Its installed package
    # does not consistently propagate this imported dependency to consumers.
    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(maze_graphical PRIVATE glfw winmm)
endif()

cmake_minimum_required(VERSION 3.24)

project(EvolvingGarden
    VERSION 0.1.0
    DESCRIPTION "A creature evolution and virtual pet simulation"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Keep third-party examples and tests out of our build.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

# raylib is fetched and built automatically on the first CMake configure.
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(EvolvingGarden
    renderer/main.cpp
)

target_link_libraries(EvolvingGarden PRIVATE raylib)

if(MSVC)
    target_compile_options(EvolvingGarden PRIVATE /W4 /permissive-)
else()
    target_compile_options(EvolvingGarden PRIVATE -Wall -Wextra -Wpedantic)
endif()

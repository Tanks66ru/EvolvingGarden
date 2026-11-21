cmake_minimum_required(VERSION 3.24)
project(EvolvingGarden VERSION 0.2.0 DESCRIPTION "A creature evolution and virtual pet simulation" LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(EvolvingGarden renderer/main.cpp)
target_include_directories(EvolvingGarden PRIVATE .)
target_link_libraries(EvolvingGarden PRIVATE raylib)

if(MSVC)
    target_compile_options(EvolvingGarden PRIVATE /W4 /permissive-)
else()
    target_compile_options(EvolvingGarden PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The simulation test target has no Raylib dependency.
add_executable(EvolvingGardenTests tests/simulation_tests.cpp)
target_include_directories(EvolvingGardenTests PRIVATE .)
if(MSVC)
    target_compile_options(EvolvingGardenTests PRIVATE /W4 /permissive-)
else()
    target_compile_options(EvolvingGardenTests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME EvolvingGardenSimulationTests COMMAND EvolvingGardenTests)

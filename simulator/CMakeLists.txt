cmake_minimum_required(VERSION 3.24)

project(EvolvingGardenSimulator VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# SDL2 is built as a static dependency so the simulator does not need a
# separately installed SDL2.dll on Windows.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.11/SDL2-2.30.11.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(SDL2)

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(LV_BUILD_CONF_DIR "" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    lvgl
    URL https://github.com/lvgl/lvgl/archive/refs/tags/v9.5.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lvgl)

# LVGL's built-in SDL driver is compiled as part of LVGL and needs SDL's
# include directories and link libraries while building the LVGL sources.
target_link_libraries(lvgl PUBLIC SDL2::SDL2-static)

set(EVOLVING_GARDEN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_executable(EvolvingGardenSimulator
    main.cpp
    ../app/creature_app.cpp
    ../platform/desktop/audio.cpp
)

target_include_directories(EvolvingGardenSimulator PRIVATE
    "${EVOLVING_GARDEN_ROOT}"
)

target_link_libraries(EvolvingGardenSimulator PRIVATE
    lvgl::lvgl
    SDL2::SDL2-static
)

if(MSVC)
    target_compile_options(EvolvingGardenSimulator PRIVATE /W4 /permissive-)
else()
    target_compile_options(EvolvingGardenSimulator PRIVATE -Wall -Wextra -Wpedantic)
endif()

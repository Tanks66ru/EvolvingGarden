cmake_minimum_required(VERSION 3.16)

# Keep the embedded application independent from the desktop Raylib target.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(evolving_garden_firmware)

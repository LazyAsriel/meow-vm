cmake_minimum_required(VERSION 3.25)
project(meow-vm VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_UNITY_BUILD "Enable Unity/Jumbo build" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF) 

configure_file(
    "${PROJECT_SOURCE_DIR}/include/meow/config.h.in"
    "${PROJECT_BINARY_DIR}/include/meow/config.h"
)

add_subdirectory(libs)

add_subdirectory(src)

# if(BUILD_BENCHMARKS)
    message(STATUS "Benchmarks enabled")
    add_subdirectory(benchmarks)
# endif()
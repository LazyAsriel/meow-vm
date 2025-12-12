project(meow-asm LANGUAGES CXX)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(masm ${SOURCES})

target_include_directories(masm PRIVATE "include")

target_compile_features(masm PRIVATE cxx_std_23)

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(MSVC)
        target_compile_options(masm PRIVATE /O2 /Oi /Ot /std:c++latest)
    else()
        target_compile_options(masm PRIVATE -O3 -march=native -flto)
        target_link_options(masm PRIVATE -flto)
    endif()
endif()
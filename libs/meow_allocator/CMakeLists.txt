cmake_minimum_required(VERSION 3.20)
project(meow_allocator LANGUAGES CXX)

add_library(meow_allocator INTERFACE)

target_include_directories(meow_allocator INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(meow_allocator INTERFACE cxx_std_23)

if(MSVC)
    target_compile_options(meow_allocator INTERFACE 
        /O2 /Ob2 /Oi /Ot /GL /Gw
    )
else()
    target_compile_options(meow_allocator INTERFACE 
        -O3 
        -march=native 
        -funroll-loops 
        -fno-math-errno
        -flto
    )
endif()

add_library(meow::allocator ALIAS meow_allocator)
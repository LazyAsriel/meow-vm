cmake_minimum_required(VERSION 3.20)

project(masm LANGUAGES CXX)

set(NO_EXCEPTION_FLAGS -fno-exceptions -fno-rtti)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(OPT_FLAGS -O3 -march=native -funroll-loops -fomit-frame-pointer -fstrict-aliasing)
    set(LTO_FLAGS -flto=thin)
else()
    set(OPT_FLAGS "")
    set(LTO_FLAGS "")
endif()

set(MASM_CXX_FLAGS ${NO_EXCEPTION_FLAGS} ${OPT_FLAGS} ${LTO_FLAGS})

add_library(masm_core OBJECT
    src/assembler.cpp
    src/lexer.cpp
    src/utils.cpp
    src/optimizer.cpp 
)

target_include_directories(masm_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(masm_core PUBLIC meow_core)
target_compile_features(masm_core PUBLIC cxx_std_23)
target_compile_options(masm_core PRIVATE ${MASM_CXX_FLAGS})

add_executable(masm src/main.cpp)

target_link_libraries(masm PRIVATE masm_core meow_core) 

target_compile_features(masm PRIVATE cxx_std_23)
target_compile_options(masm PRIVATE ${MASM_CXX_FLAGS})
target_link_options(masm PRIVATE ${NO_EXCEPTION_FLAGS} ${LTO_FLAGS} -Wl,--gc-sections)

add_executable(test_optimizer tests/test_optimizer.cpp)

target_link_libraries(test_optimizer PRIVATE masm_core meow_core)

target_compile_features(test_optimizer PRIVATE cxx_std_23)
target_compile_options(test_optimizer PRIVATE ${MASM_CXX_FLAGS})
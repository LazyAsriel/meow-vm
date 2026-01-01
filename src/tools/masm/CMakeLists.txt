cmake_minimum_required(VERSION 3.20)

project(masm LANGUAGES CXX)

# --- CẤU HÌNH TỐI ƯU HÓA ---
set(NO_EXCEPTION_FLAGS 
    -fno-exceptions 
    -fno-rtti
)

set(OPT_FLAGS
    -O3
    -march=native
    -funroll-loops
    -fomit-frame-pointer
    -fstrict-aliasing
)

set(LTO_FLAGS -flto=thin)
set(MASM_CXX_FLAGS ${NO_EXCEPTION_FLAGS} ${OPT_FLAGS} ${LTO_FLAGS})

# --- LIBRARY (CORE) ---

add_library(masm_core OBJECT
    src/assembler.cpp
    src/lexer.cpp
    src/utils.cpp
)

target_include_directories(masm_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(masm_core PUBLIC meow_core)

target_compile_features(masm_core PUBLIC cxx_std_23)

# [QUAN TRỌNG] Đổi PUBLIC thành PRIVATE để không ép meow-vm tắt exception
target_compile_options(masm_core PRIVATE ${MASM_CXX_FLAGS})

# --- EXECUTABLE (CLI) ---

add_executable(masm src/main.cpp)

target_link_libraries(masm PRIVATE masm_core)
target_compile_features(masm PRIVATE cxx_std_23)

# Áp dụng cờ tối ưu riêng cho executable masm (nơi chúng ta kiểm soát code)
target_compile_options(masm PRIVATE ${MASM_CXX_FLAGS})
target_link_options(masm PRIVATE ${NO_EXCEPTION_FLAGS} ${LTO_FLAGS} -Wl,--gc-sections)
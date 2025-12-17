# [src/CMakeLists.txt]

# 1. Bỏ "jit/*.cpp" và "jit/*.h" trong GLOB_RECURSE
file(GLOB_RECURSE CORE_SOURCES 
    "vm/*.cpp"          "vm/*.h"
    "bytecode/*.cpp"    "bytecode/*.h"
    "memory/*.cpp"      "core/*.cpp"
    "runtime/*.cpp"     "runtime/*.h"
    "module/*.cpp"      
    "diagnostics/*.cpp"
)

# Tạo thư viện Core
add_library(meow_core OBJECT ${CORE_SOURCES})

target_include_directories(meow_core 
    PUBLIC  "${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(meow_core PRIVATE meow::libs)
target_compile_options(meow_core PRIVATE -Wno-unused-parameter)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")
    target_precompile_headers(meow_core PRIVATE "pch.h")
endif()

# 2. Thêm thư mục JIT
add_subdirectory(jit)

# Executable (CLI)
add_executable(meow-vm "cli/main.cpp")

# 3. Link thêm meow_jit vào meow-vm
target_link_libraries(meow-vm PRIVATE meow_core meow_jit masm_core meow::libs)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")
    target_precompile_headers(meow-vm PRIVATE "pch.h")
endif()

# Tools
add_subdirectory(tools/masm)
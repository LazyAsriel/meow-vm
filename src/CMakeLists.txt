# Gom source code
file(GLOB_RECURSE CORE_SOURCES 
    "vm/*.cpp"          "vm/*.h"
    "compiler/*.cpp"    "compiler/*.h"
    "memory/*.cpp"      "core/*.cpp"
    "runtime/*.cpp"     "runtime/*.h"
    "jit/*.h"           "module/*.cpp"      
    "diagnostics/*.cpp"
)

# Tạo thư viện Core
add_library(meow_core OBJECT ${CORE_SOURCES})

target_include_directories(meow_core 
    PUBLIC  "${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(meow_core PRIVATE meow::libs)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")
    target_precompile_headers(meow_core PRIVATE "pch.h")
endif()

# Executable (CLI)
add_executable(meow-vm "cli/main.cpp")
target_link_libraries(meow-vm PRIVATE meow_core meow::libs)
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")
    target_precompile_headers(meow-vm PRIVATE "pch.h")
endif()

# Tools
add_subdirectory(tools/masm)

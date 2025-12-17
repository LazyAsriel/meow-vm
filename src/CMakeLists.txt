file(GLOB_RECURSE CORE_SOURCES 
    "vm/*.cpp"          "vm/*.h"
    "bytecode/*.cpp"    "bytecode/*.h"
    "memory/*.cpp"      "core/*.cpp"
    "runtime/*.cpp"     "runtime/*.h"
    "module/*.cpp"      
    "diagnostics/*.cpp"
)

add_library(meow_core OBJECT ${CORE_SOURCES})

target_include_directories(meow_core 
    PUBLIC  "${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(meow_core PUBLIC meow::libs)
target_compile_options(meow_core PRIVATE -Wno-unused-parameter)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")
    target_precompile_headers(meow_core PRIVATE "pch.h")
endif()

# add_subdirectory(jit)
add_executable(meow-vm "cli/main.cpp")
target_link_libraries(meow-vm PRIVATE meow_core masm_core meow::libs)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/pch.h")
    target_precompile_headers(meow-vm PRIVATE "pch.h")
endif()

add_subdirectory(tools/masm)
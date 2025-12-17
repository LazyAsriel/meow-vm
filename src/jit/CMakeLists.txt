add_library(meow_jit OBJECT
    # Public API
    jit_compiler.cpp
    
    # Frontend (Analysis)
    analysis/bytecode_analysis.cpp
    
    # Backend (x64)
    x64/assembler.cpp
    x64/code_generator.cpp
    
    # Runtime Support
    runtime/runtime_stubs.cpp
    runtime/deopt.cpp
)

# Include directories
target_include_directories(meow_jit PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include 
)

# Sử dụng chuẩn C++23 giống masm
target_compile_features(meow_jit PUBLIC cxx_std_23)
target_link_libraries(meow_jit PUBLIC meow_core) 
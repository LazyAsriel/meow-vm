add_library(masm_core STATIC
    src/assembler.cpp
    src/lexer.cpp
)

target_include_directories(masm_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(masm_core PUBLIC meow_core)

target_compile_features(masm_core PUBLIC cxx_std_23)

add_executable(masm src/main.cpp)

target_link_libraries(masm PRIVATE masm_core)
target_compile_features(masm PRIVATE cxx_std_23)
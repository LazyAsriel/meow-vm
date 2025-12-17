add_subdirectory("${PROJECT_SOURCE_DIR}/src/jit" meow_jit_build)
add_executable(bench_native "bench_native.cpp")
target_compile_features(bench_native PRIVATE cxx_std_23)
target_link_libraries(bench_native PRIVATE 
    meow_jit
    meow_core 
    meow::libs
)

target_include_directories(bench_native PRIVATE "${PROJECT_SOURCE_DIR}/src")
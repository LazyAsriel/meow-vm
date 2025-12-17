add_executable(meow_dispatch_bench "dispatch_bench.cpp")
target_compile_features(meow_dispatch_bench PRIVATE cxx_std_23)
target_link_libraries(meow_dispatch_bench PRIVATE meow_core meow::libs)
target_include_directories(meow_dispatch_bench PRIVATE "${PROJECT_SOURCE_DIR}/src")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/jit" meow_jit_build)
add_executable(meow_bench_vs_native "vm_vs_native.cpp")
target_compile_features(meow_bench_vs_native PRIVATE cxx_std_23)
target_link_libraries(meow_bench_vs_native PRIVATE 
    meow_jit
    meow_core 
    meow::libs
)

target_include_directories(meow_bench_vs_native PRIVATE "${PROJECT_SOURCE_DIR}/src")
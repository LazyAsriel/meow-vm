# Benchmark Dispatch
add_executable(meow_dispatch_bench "dispatch_bench.cpp")
target_compile_features(meow_dispatch_bench PRIVATE cxx_std_23)
target_link_libraries(meow_dispatch_bench PRIVATE meow_core meow::libs)
# Thêm src vào include path để benchmark truy cập được internal headers (như vm/interpreter.h)
target_include_directories(meow_dispatch_bench PRIVATE "${PROJECT_SOURCE_DIR}/src")

# Benchmark VM vs Native
add_executable(meow_bench_vs_native "vm_vs_native.cpp")
target_compile_features(meow_bench_vs_native PRIVATE cxx_std_23)
target_link_libraries(meow_bench_vs_native PRIVATE meow_core meow::libs)
target_include_directories(meow_bench_vs_native PRIVATE "${PROJECT_SOURCE_DIR}/src")

# Optimization
if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(meow_dispatch_bench PRIVATE -O3 -march=native)
    target_compile_options(meow_bench_vs_native PRIVATE -O3 -march=native)
endif()

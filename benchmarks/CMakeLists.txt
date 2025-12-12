# Benchmark Dispatch (So sánh các kỹ thuật Interpreter)
add_executable(meow_dispatch_bench "dispatch_bench.cpp")
target_compile_features(meow_dispatch_bench PRIVATE cxx_std_23)
# Link với meow_core (thư viện nội bộ) và meow::libs (thư viện ngoài)
target_link_libraries(meow_dispatch_bench PRIVATE meow_core meow::libs)

# Benchmark VM vs Native
add_executable(meow_bench_vs_native "vm_vs_native.cpp")
target_compile_features(meow_bench_vs_native PRIVATE cxx_std_23)
target_link_libraries(meow_bench_vs_native PRIVATE meow_core meow::libs)

# Tối ưu hóa cho Benchmark (Chỉ bật khi Release để đo cho chuẩn)
if(NOT MSVC AND CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(meow_dispatch_bench PRIVATE -O3 -march=native)
    target_compile_options(meow_bench_vs_native PRIVATE -O3 -march=native)
endif()

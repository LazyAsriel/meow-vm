cmake_minimum_required(VERSION 3.25)
project(meow-vm LANGUAGES CXX)

# --- 1. Project Configuration ---
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
option(ENABLE_UNITY_BUILD "Enable Unity/ Jumbo build" ON)
option(MEOW_STD_SHARED "Build stdlib as shared lib" OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- 2. Setup Runtime Environment ---
set(MEOW_ROOT_CONTENT "$ORIGIN/..")
set(MEOW_ROOT_FILE "${CMAKE_BINARY_DIR}/bin/meow-root")
file(WRITE ${MEOW_ROOT_FILE} "${MEOW_ROOT_CONTENT}\n")

# --- 3. Add Libraries ---
add_subdirectory(libs)

# --- 4. VM CORE (QUAN TRỌNG: Tách code lõi ra) ---
# Lấy tất cả file cpp trong src
file(GLOB_RECURSE VM_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# Loại bỏ src/main.cpp ra khỏi danh sách này (vì nó chứa hàm main)
# Để chúng ta có thể tái sử dụng VM_SOURCES cho benchmark
list(FILTER VM_SOURCES EXCLUDE REGEX "src/main.cpp$")

# Tạo một Object Library cho VM Core để không phải compile lại nhiều lần
add_library(meow_vm_core OBJECT ${VM_SOURCES})
target_include_directories(meow_vm_core PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(meow_vm_core PRIVATE meow::libs)

# Setup Unity Build cho Core
if (ENABLE_UNITY_BUILD)
    set_property(TARGET meow_vm_core PROPERTY UNITY_BUILD ON)
endif()

# Setup PCH cho Core
set(PCH_HEADER "${PROJECT_SOURCE_DIR}/include/common/pch.h")
if (EXISTS "${PCH_HEADER}")
    target_precompile_headers(meow_vm_core PRIVATE "${PCH_HEADER}")
endif()

# --- 5. Main VM Executable ---
# Executable chính chỉ gồm src/main.cpp + cục core ở trên
add_executable(${PROJECT_NAME} "src/main.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE meow_vm_core meow::libs)
# (Kế thừa PCH từ meow_vm_core tự động nếu link public, hoặc set lại nếu cần)
if (EXISTS "${PCH_HEADER}")
    target_precompile_headers(${PROJECT_NAME} PRIVATE "${PCH_HEADER}")
endif()

# --- 6. Tools (MASM) ---
add_executable(masm "tools/masm.cpp")
target_compile_features(masm PRIVATE cxx_std_23)
# target_link_libraries(masm PRIVATE meow::libs) # Uncomment nếu cần

# --- 7. Standard Library ---
file(GLOB STD_SOURCES CONFIGURE_DEPENDS "stdlib/src/*.cpp")
if (STD_SOURCES)
    add_library(meow_std_objects OBJECT ${STD_SOURCES})
    target_include_directories(meow_std_objects PUBLIC "${PROJECT_SOURCE_DIR}/include")
    target_compile_definitions(meow_std_objects PRIVATE MEOW_STDLIB_EXPORTS)
    target_link_libraries(meow_std_objects PRIVATE meow::libs)
    if (ENABLE_UNITY_BUILD)
        set_property(TARGET meow_std_objects PROPERTY UNITY_BUILD ON)
    endif()
    
    target_link_libraries(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:meow_std_objects>)
endif()

# --- 8. Benchmarks ---

# Fix Lỗi 1: Không dùng GLOB bừa bãi cho benchmark nữa.
# Fix Lỗi 2: Link với meow_vm_core để thấy được Machine/Interpreter.

# Benchmark cũ (nếu cậu muốn giữ)
if(EXISTS "benchmarks/main.cpp")
    add_executable(meow_bench_old "benchmarks/main.cpp")
    target_compile_features(meow_bench_old PRIVATE cxx_std_23)
    target_link_libraries(meow_bench_old PRIVATE meow_vm_core meow::libs)
    if(NOT MSVC)
        target_compile_options(meow_bench_old PRIVATE -O3 -march=native)
    endif()
endif()

# Benchmark mới (Dispatch War)
add_executable(meow_dispatch_bench "benchmarks/dispatch_bench.cpp")
target_compile_features(meow_dispatch_bench PRIVATE cxx_std_23)

# LIÊN KẾT VỚI CORE ĐỂ HẾT LỖI UNDEFINED REFERENCE
target_link_libraries(meow_dispatch_bench PRIVATE meow_vm_core meow::libs)

if(NOT MSVC)
    # Build release/optimized cho benchmark (nhưng bỏ -flto để debug cho dễ nếu cần)
    target_compile_options(meow_dispatch_bench PRIVATE -O3 -march=native)
endif()

# Benchmark so sánh VM vs Native
add_executable(meow_bench_vs_native "benchmarks/vm_vs_native.cpp")
target_compile_features(meow_bench_vs_native PRIVATE cxx_std_23)

# Link với Core để chạy được VM
target_link_libraries(meow_bench_vs_native PRIVATE meow_vm_core meow::libs)

if(NOT MSVC)
    # Tối ưu hết mức để Native C++ chạy nhanh nhất có thể
    target_compile_options(meow_bench_vs_native PRIVATE -O3 -march=native)
endif()

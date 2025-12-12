project(meow-asm LANGUAGES CXX)
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(masm ${SOURCES})

# Include cả thư mục gốc để dùng chung OpCode
target_include_directories(masm PRIVATE 
    "include" 
    "${PROJECT_SOURCE_DIR}/src/compiler" # Để include op_codes.h
)
target_compile_features(masm PRIVATE cxx_std_23)

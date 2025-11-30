add_library(meow_allocator INTERFACE)

target_include_directories(meow_allocator INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(meow_allocator INTERFACE cxx_std_23)
add_library(meow::allocator ALIAS meow_allocator)
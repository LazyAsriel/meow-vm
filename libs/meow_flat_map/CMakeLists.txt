add_library(meow_flat_map INTERFACE)

target_include_directories(meow_flat_map INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(meow_flat_map INTERFACE cxx_std_23)
target_link_libraries(meow_flat_map INTERFACE meow::allocator) 
add_library(meow::flat_map ALIAS meow_flat_map)
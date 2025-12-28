add_library(meow_hash_map INTERFACE)

target_include_directories(meow_hash_map INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(meow_hash_map INTERFACE cxx_std_23)

target_link_libraries(meow_hash_map INTERFACE meow::allocator)

add_library(meow::hash_map ALIAS meow_hash_map)
add_subdirectory(meow_variant)
add_subdirectory(meow_allocator)
add_subdirectory(meow_flat_map)
add_subdirectory(meow_hash_map)
add_subdirectory(meow_enum)
add_subdirectory(meow_zerr)

add_library(meow_libs INTERFACE)

target_link_libraries(meow_libs INTERFACE
    meow::variant
    meow::allocator
    meow::flat_map
    meow::hash_map
    meow::enum_utils
    meow::zerr
)

add_library(meow::libs ALIAS meow_libs)
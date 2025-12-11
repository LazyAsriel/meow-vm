add_subdirectory(meow_variant)
add_subdirectory(meow_allocator)
add_subdirectory(meow_flat_map)
add_subdirectory(meow_expected)
add_subdirectory(meow_optional)

add_library(meow_libs INTERFACE)

target_link_libraries(meow_libs INTERFACE
    meow::variant
    meow::allocator
    meow::expected
    meow::optional
)

add_library(meow::libs ALIAS meow_libs)
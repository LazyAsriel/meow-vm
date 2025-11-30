add_library(meow_optional INTERFACE)

target_include_directories(meow_optional INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(meow_optional INTERFACE cxx_std_23)
target_link_libraries(meow_optional INTERFACE meow::variant)
add_library(meow::optional ALIAS meow_optional)
add_library(meow_expected INTERFACE)

target_include_directories(meow_expected INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(meow_expected INTERFACE cxx_std_23)
target_link_libraries(meow_expected INTERFACE meow::variant)
add_library(meow::expected ALIAS meow_expected)
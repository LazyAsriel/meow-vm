add_library(meow_variant INTERFACE)

target_include_directories(meow_variant INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(meow_variant INTERFACE cxx_std_23)
add_library(meow::variant ALIAS meow_variant)
add_library(meow_zerr INTERFACE)

target_include_directories(meow_zerr INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(meow_zerr INTERFACE cxx_std_23)

add_library(meow::zerr ALIAS meow_zerr)
add_library(meow_enum INTERFACE)

target_include_directories(meow_enum INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(meow_enum INTERFACE cxx_std_23)
add_library(meow::enum_utils ALIAS meow_enum)
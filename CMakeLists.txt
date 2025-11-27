cmake_minimum_required(VERSION 3.25)
project(meow-vm LANGUAGES CXX)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

option(ENABLE_UNITY_BUILD "Enable Unity/ Jumbo build to reduce compiler overhead" ON)
option(MEOW_STD_SHARED "Build stdlib as a shared library instead of linking object library into executable" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(MEOW_ROOT_CONTENT "$ORIGIN/..")
set(MEOW_ROOT_FILE "${CMAKE_BINARY_DIR}/bin/meow-root")
file(WRITE ${MEOW_ROOT_FILE} "${MEOW_ROOT_CONTENT}\n")

# ==============================================================================
# [NEW] AUTO-DISCOVERY LIBRARIES
# Tự động tìm các folder trong libs/ có chứa CMakeLists.txt và add_subdirectory
# ==============================================================================
file(GLOB children RELATIVE ${CMAKE_SOURCE_DIR}/libs ${CMAKE_SOURCE_DIR}/libs/*)
foreach(child ${children})
    if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libs/${child} AND EXISTS ${CMAKE_SOURCE_DIR}/libs/${child}/CMakeLists.txt)
        message(STATUS "📦 Library Found: ${child}")
        add_subdirectory(libs/${child})
    endif()
endforeach()

# ==============================================================================
# MAIN TARGETS
# ==============================================================================

file(GLOB_RECURSE VM_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
add_executable(${PROJECT_NAME} ${VM_SOURCES})

add_executable(masm "tools/masm.cpp")
target_compile_features(masm PRIVATE cxx_std_23)

target_include_directories(${PROJECT_NAME} PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
    # "${PROJECT_SOURCE_DIR}/libs" <-- ĐÃ XÓA: Không cần dòng này nữa
)

# [NEW] Link thư viện meow::variant vào VM
# CMake sẽ tự động thêm include path của variant vào VM
target_link_libraries(${PROJECT_NAME} PRIVATE meow::variant)

# --- Precompiled Headers (PCH) ---
set(PCH_HEADER "${PROJECT_SOURCE_DIR}/include/common/pch.h")
if (EXISTS "${PCH_HEADER}")
    message(STATUS "PCH: Found ${PCH_HEADER} -> Enabling precompiled headers.")
    target_precompile_headers(${PROJECT_NAME} PRIVATE "${PCH_HEADER}")
else()
    message(STATUS "PCH: ${PCH_HEADER} not found -> Precompiled headers disabled.")
endif()

file(GLOB STD_SOURCES CONFIGURE_DEPENDS "stdlib/src/*.cpp")

if (STD_SOURCES)
    add_library(meow_std_objects OBJECT ${STD_SOURCES})

    target_include_directories(meow_std_objects PUBLIC
        "${PROJECT_SOURCE_DIR}/stdlib/common"
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/include/common"
        "${PROJECT_SOURCE_DIR}/include/runtime"
        "${PROJECT_SOURCE_DIR}/include/vm"
        "${PROJECT_SOURCE_DIR}/include/memory"
        "${PROJECT_SOURCE_DIR}/include/loader"
        "${PROJECT_SOURCE_DIR}/include/module"
    )

    target_compile_definitions(meow_std_objects PRIVATE MEOW_STDLIB_EXPORTS)
    
    # [NEW] Stdlib cũng cần biết cấu trúc của Variant
    target_link_libraries(meow_std_objects PRIVATE meow::variant)

    if (EXISTS "${PCH_HEADER}")
        target_precompile_headers(meow_std_objects PRIVATE "${PCH_HEADER}")
    endif()

    if (MEOW_STD_SHARED)
        add_library(meow_std_shared SHARED $<TARGET_OBJECTS:meow_std_objects>)
        set_target_properties(meow_std_shared PROPERTIES
            PREFIX ""
            LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/stdlib"
        )
        target_link_libraries(${PROJECT_NAME} PRIVATE meow_std_shared)
    else()
        target_link_libraries(${PROJECT_NAME} PRIVATE $<TARGET_OBJECTS:meow_std_objects>)
    endif()
endif()

if (ENABLE_UNITY_BUILD)
    message(STATUS "UNITY_BUILD: Enabled for ${PROJECT_NAME} (may greatly speed up builds).")
    set_property(TARGET ${PROJECT_NAME} PROPERTY UNITY_BUILD ON)
    if (TARGET meow_std_objects)
        set_property(TARGET meow_std_objects PROPERTY UNITY_BUILD ON)
    endif()
else()
    message(STATUS "UNITY_BUILD: Disabled.")
endif()

# --- Helpful defaults: allow overriding JOBS env if provided ---
if(NOT DEFINED ENV{JOBS})
    set(ENV{JOBS} "${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()
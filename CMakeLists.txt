cmake_minimum_required(VERSION 3.25)

project(perfoverlay
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive-)
endif()

# Core static library (OS-agnostic, unit-testable)
add_library(perfoverlay_core STATIC
    src/core/frame_stats.cpp
)
target_include_directories(perfoverlay_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Sensors static library (Hardware telemetry)
add_library(perfoverlay_sensors STATIC
    src/sensors/placeholder.cpp
)
target_include_directories(perfoverlay_sensors PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(perfoverlay_sensors PUBLIC
    perfoverlay_core
)

# Main executable
add_executable(perfoverlay
    src/main.cpp
)
target_link_libraries(perfoverlay PRIVATE
    perfoverlay_core
    perfoverlay_sensors
)

# Tests
enable_testing()
find_package(Catch2 3 REQUIRED)

add_executable(perfoverlay_tests
    tests/test_frame_stats.cpp
)
target_link_libraries(perfoverlay_tests PRIVATE
    perfoverlay_core
    Catch2::Catch2WithMain
)
target_include_directories(perfoverlay_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

include(CTest)
include(Catch)
catch_discover_tests(perfoverlay_tests)

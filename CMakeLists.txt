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

find_package(imgui CONFIG REQUIRED)

# Core static library (OS-agnostic, unit-testable)
add_library(perfoverlay_core STATIC
    src/core/frame_stats.cpp
    src/core/metrics_hub.cpp
)
target_include_directories(perfoverlay_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Sensors static library (Hardware telemetry)
add_library(perfoverlay_sensors STATIC
    src/sensors/cpu_sensor.cpp
    src/sensors/amd_gpu_sensor.cpp
    src/sensors/gpu_sensor_fallback.cpp
    src/sensors/sensor_manager.cpp
    third_party/ADLX/SDK/ADLXHelper/Windows/Cpp/ADLXHelper.cpp
    third_party/ADLX/SDK/Platform/Windows/WinAPIs.cpp
)

if(MSVC)
    set_source_files_properties(
        third_party/ADLX/SDK/ADLXHelper/Windows/Cpp/ADLXHelper.cpp
        third_party/ADLX/SDK/Platform/Windows/WinAPIs.cpp
        PROPERTIES COMPILE_FLAGS "/W3 /wd4100 /wd4201 /wd4505 /wd4189"
    )
endif()

target_include_directories(perfoverlay_sensors PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(perfoverlay_sensors SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ADLX/SDK
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ADLX/SDK/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ADLX/SDK/ADLXHelper/Windows/Cpp
)
target_link_libraries(perfoverlay_sensors PUBLIC
    perfoverlay_core
    pdh
)

# Capture static library (ETW PresentMon frame capture)
add_library(perfoverlay_capture STATIC
    src/capture/frame_time_calculator.cpp
    src/capture/foreground_tracker.cpp
    src/capture/etw_present_capture.cpp
)
target_include_directories(perfoverlay_capture PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(perfoverlay_capture PUBLIC
    perfoverlay_core
    advapi32
)

# UI static library (D3D11 + ImGui transparent overlay)
add_library(perfoverlay_ui STATIC
    src/ui/overlay_window.cpp
)
target_include_directories(perfoverlay_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(perfoverlay_ui PUBLIC
    perfoverlay_core
    imgui::imgui
    d3d11
    dxgi
    dwmapi
)

# Upscaler static library (File-based upscaler detection: DLSS, FSR, XeSS)
add_library(perfoverlay_upscaler STATIC
    src/upscaler/filesystem.cpp
    src/upscaler/process_path.cpp
    src/upscaler/upscaler_detector.cpp
)
target_include_directories(perfoverlay_upscaler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(perfoverlay_upscaler PUBLIC
    perfoverlay_core
)

# Main executable
add_executable(perfoverlay
    src/main.cpp
)
target_link_libraries(perfoverlay PRIVATE
    perfoverlay_core
    perfoverlay_sensors
    perfoverlay_capture
    perfoverlay_upscaler
    perfoverlay_ui
)

# Tests
enable_testing()
find_package(Catch2 3 REQUIRED)

add_executable(perfoverlay_tests
    tests/test_frame_stats.cpp
    tests/test_sensors.cpp
    tests/test_capture.cpp
    tests/test_metrics_hub.cpp
    tests/test_upscaler.cpp
)
target_link_libraries(perfoverlay_tests PRIVATE
    perfoverlay_core
    perfoverlay_sensors
    perfoverlay_capture
    perfoverlay_upscaler
    perfoverlay_ui
    Catch2::Catch2WithMain
)
target_include_directories(perfoverlay_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

include(CTest)
include(Catch)
catch_discover_tests(perfoverlay_tests)

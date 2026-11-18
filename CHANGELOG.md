# Changelog

All notable changes to **perfoverlay** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [0.1.0] - 2026-11-18

### Added
- **Core Frame Statistics Engine**:
  - `FrameTimeBuffer` fixed-capacity circular ring buffer with timestamp-based sample pruning.
  - Accurate calculation of Average FPS, Current FPS, Min/Max FPS, 1% Low FPS, 0.1% Low FPS, and 99th percentile frame times.
  - Mathematically sound 1% Low defined as the reciprocal of the mean frame time of the slowest 1% frame sample slice.
- **Zero-Injection Real-Time Frame Capture**:
  - ETW (`Event Tracing for Windows`) session capturing real-time `Microsoft-Windows-DxgKrnl` and `Microsoft-Windows-DXGI` present events without touching or injecting game code.
  - `ForegroundProcessTracker` monitoring the active foreground window and mapping presentation events to the target game.
  - Graceful permission detection for Administrator / `Performance Log Users` group.
- **Hardware Telemetry Engine**:
  - AMD ADLX SDK dynamic loading (`amdadlx64.dll`) reading GPU utilization, temperature, VRAM usage, clock frequency, and total board power.
  - Windows Performance Data Helper (PDH) fallback GPU engine counters.
  - CPU utilization monitoring via PDH total processor time.
  - Safe, non-invasive CPU temperature monitoring via optional LibreHardwareMonitor Web Server integration (`http://localhost:8085/data.json`) with zero kernel driver installations.
  - Asynchronous 500 ms sensor polling orchestrator powered by `std::jthread`.
- **Passive Upscaler Detection**:
  - Anti-cheat-safe file-system scanning identifying active game libraries: NVIDIA DLSS (`nvngx_dlss.dll`), DLSS Frame Generation (`nvngx_dlssg.dll`), AMD FSR 1/2/3 (`amd_fidelityfx_*.dll`), and Intel XeSS (`libxess.dll`).
  - Read-only process query via `PROCESS_QUERY_LIMITED_INFORMATION` without memory access (`PROCESS_VM_READ`).
  - Fast memory-cached directory scanning preventing per-frame disk I/O.
- **Central MetricsHub**:
  - Lock-free / mutex-buffered snapshot aggregation uniting frame stats, sensors, and upscaler info.
  - Clear state identification for active game detection vs. desktop idle.
- **External Transparent Overlay UI**:
  - Always-on-top, borderless, click-through Win32 window (`WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE`).
  - Direct3D 11 swapchain with DWM per-pixel composition transparency.
  - Dear ImGui rendering displaying FPS, frame timing, 1% / 0.1% Lows, hardware utilization, temperatures, and upscaler badges.
  - Global `F10` hotkey for instant overlay toggle.
- **Configuration & Usability**:
  - JSON configuration stored at `%APPDATA%\perfoverlay\config.json`.
  - Configurable screen corner anchoring (`TopLeft`, `TopRight`, `BottomLeft`, `BottomRight`), background opacity, font size scale, update intervals, and fine-grained metric visibility.
  - Automatic default creation, partial JSON merging, and automatic corrupted-file recovery with `.bak` backups.
  - Windows Notification Area (System Tray) icon with right-click menu (Toggle Overlay, Settings, Open Config File, Exit).
  - Standalone interactive Dear ImGui `SettingsWindow` accessible via tray menu or `--settings` CLI flag.
- **Build & CI / Packaging**:
  - Pure C++20 and CMake 3.25+ build system with vcpkg manifest mode.
  - Static CRT (`/MT`) linkage for self-contained single-executable distribution.
  - Comprehensive unit test suite with 45 Catch2 tests covering math, sensors, ETW parsers, config recovery, and upscaler scanners.
  - GitHub Actions CI workflow verifying MSVC build, tests, and clang-format style checks.
  - Automated GitHub Release packaging workflow on `v*` tag pushes.

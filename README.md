# perfoverlay

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11%20x64-0078D6?logo=windows)](https://microsoft.com/windows)
[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-00599C?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)

**perfoverlay** is an open-source, single-executable, external hardware telemetry and FPS overlay for Windows. Designed as a modern, anti-cheat-safe alternative to MSI Afterburner and RivaTuner Statistics Server (RTSS), it operates with **strictly zero game process DLL injection and zero API hooking**.

Built from the ground up using **C++20**, **Direct3D 11**, **Dear ImGui**, and native **Windows ETW (Event Tracing for Windows)** architecture.

---

## Preview

```text
+-------------------------------------------------------------+
| [Waiting for game...]                                       |
| FPS: 142.5  |  Frametime: 7.02 ms                           |
| 1% Low: 104.2 FPS  |  0.1% Low: 78.5 FPS                    |
| CPU: 24.3% (58°C)  |  GPU: 94.1% (64°C)                     |
| VRAM: 9840 / 16384 MB  |  Clock: 2450 MHz  |  Power: 215 W  |
| Upscaler: DLSS (FrameGen), FSR 3 [Detected in game files]   |
+-------------------------------------------------------------+
```
*(Preview: Transparent, click-through Direct3D 11 overlay window rendered seamlessly over games)*

---

## Feature Matrix

| Feature / Metric | Description | Provider & Method | Anti-Cheat Impact |
| :--- | :--- | :--- | :--- |
| **FPS & Frame Time** | Real-time frame rate and presentation delta (ms) | Windows ETW (`DxgKrnl` / `DXGI`) | **Zero Injection** (Out-of-process kernel trace) |
| **1% & 0.1% Low FPS** | Reciprocal of mean frame time of worst 1% and 0.1% | `FrameTimeBuffer` circular queue | Pure mathematical calculation |
| **GPU Usage & Temp** | Core load (%) and junction/edge temperature (°C) | AMD ADLX SDK (Fallback: Windows PDH) | Official hardware API |
| **VRAM, Clocks & Power** | Used/Total VRAM (MB), Core Clock (MHz), Board Power (W) | AMD ADLX SDK | Official hardware API |
| **CPU Usage** | Total CPU processor utilization (%) | Windows Performance Data Helper (PDH) | Official Windows API |
| **CPU Temperature** | Processor package temperature (°C) | LibreHardwareMonitor Web Server (`localhost:8085`) | **Driverless** (No vulnerable kernel drivers) |
| **Upscaler Detection** | Presence of DLSS, DLSS-G, FSR, and XeSS libraries | Passive file-system scanner | Read-only disk inspection |
| **Overlay Window** | Transparent, always-on-top, click-through UI | Win32 + D3D11 + DWM + Dear ImGui | Never touches game memory |
| **System Tray & Settings** | Taskbar notification icon and interactive dialog | Windows Shell Notify + ImGui Dialog | Standalone Win32 window |

---

## "Zero-Injection" & Anti-Cheat Safety

> [!IMPORTANT]
> **Honest Disclosure**: `perfoverlay` dramatically reduces the risk of bans compared to traditional overlay tools (RTSS, SpecialK, ReShade, etc.) by never injecting code into game address spaces. However, no third-party software can offer a **100% absolute guarantee** against kernel-level (Ring-0) anti-cheat heuristics. Use at your own discretion.

### Why is it significantly safer?
1. **NO DLL Injection**: No foreign DLLs are injected into the game's virtual address space (`CreateRemoteThread`, `SetWindowsHookEx`, and manual DLL injection techniques are strictly forbidden).
2. **NO API Hooking**: Function pointers (`Present`, `Present1`, `ResizeBuffers`) in DirectX and Vulkan virtual method tables (VTable / IAT) are completely untouched.
3. **ETW (Event Tracing for Windows) Approach**: Presentation timestamps are captured externally out-of-process via standard Windows kernel event trace sessions (`Microsoft-Windows-DxgKrnl` and `Microsoft-Windows-DXGI`), exactly like Microsoft's Windows Performance Analyzer and Intel PresentMon.
4. **Least-Privilege Queries**: When inspecting active game executables, only `PROCESS_QUERY_LIMITED_INFORMATION` is requested. Memory read permissions (`PROCESS_VM_READ`) are never requested.
5. **Driverless Architecture**: Unlike traditional hardware tools that install vulnerable third-party kernel drivers (e.g. WinRing0, RTCore64), `perfoverlay` requires no driver installations, ensuring complete compatibility with Windows 11 Memory Integrity (HVCI).

---

## Hotkeys & Usage

| Action | Method |
| :--- | :--- |
| **Toggle Overlay Visibility** | Global `F10` hotkey |
| **System Tray Menu** | Right-click the notification icon in the taskbar: *Show / Hide Overlay*, *Settings...*, *Open Config File*, *Exit* |
| **Interactive Settings Dialog** | Tray icon menu -> "Settings..." or launch via CLI: `perfoverlay.exe --settings` |
| **Console Diagnostics** | Run from terminal: `perfoverlay.exe --debug` |
| **Headless (No GUI) Mode** | For benchmarking and automated tests: `perfoverlay.exe --headless` |
| **Exit After N Frames** | `perfoverlay.exe -n 120` (Automatically exits after rendering 120 frames) |

---

## Known Limitations

1. **Exclusive Fullscreen Is Not Supported**:
   - `perfoverlay` renders via the Windows Desktop Window Manager (DWM) composition layer rather than hooking the game's direct swapchain.
   - Games must be configured to run in **Borderless Windowed** or **Windowed** mode. Exclusive Fullscreen bypasses DWM composition, preventing external overlay windows from displaying on top.
2. **Upscaler Detection Scope**:
   - `perfoverlay` passively scans the active game directory for vendor upscaler libraries (`nvngx_dlss.dll`, `amd_fidelityfx_*.dll`, `libxess.dll`).
   - The UI displays **"Detected in game files"** because game memory is not read to determine if the feature is actively toggled in the game's graphics menu.
3. **CPU Temperature Requires External Server**:
   - To preserve anti-cheat safety and avoid loading kernel drivers, `perfoverlay` queries the open-source **LibreHardwareMonitor** remote web server (`http://localhost:8085`). If the server is offline, CPU temperature gracefully displays as `N/A`.

---

## Configuration (`config.json`)

User preferences are stored at `%APPDATA%\perfoverlay\config.json`:

```json
{
  "corner": "TopLeft",
  "offsetX": 20,
  "offsetY": 20,
  "windowWidth": 340,
  "windowHeight": 290,
  "backgroundAlpha": 0.78,
  "fontScale": 1.0,
  "fpsPollIntervalMs": 250,
  "sensorPollIntervalMs": 500,
  "toggleHotkey": "F10",
  "cpuTempProvider": "auto",
  "metrics": {
    "showFps": true,
    "showFrameTime": true,
    "show1PercentLow": true,
    "show01PercentLow": true,
    "showCpuUsage": true,
    "showCpuTemp": true,
    "showGpuUsage": true,
    "showGpuTemp": true,
    "showVram": true,
    "showGpuClock": true,
    "showGpuPower": true,
    "showUpscaler": true
  }
}
```

- **Corrupted File Protection**: If the configuration file becomes corrupted or invalid, `perfoverlay` automatically creates a timestamped `.bak` backup and restores clean defaults without crashing.

---

## Troubleshooting

### 1. ETW Permission Error (Win32 Error 5 / Access Denied)
Capturing real-time presentation events requires administrative privileges or membership in the Performance Log Users group:
- **Option 1**: Run `perfoverlay.exe` as Administrator (**Run as Administrator**).
- **Option 2 (Non-Admin Setup)**: Add your Windows user account to the `Performance Log Users` local group once:
  ```cmd
  net localgroup "Performance Log Users" "%USERNAME%" /add
  ```
  *(Run this command once in an elevated Command Prompt, then sign out and sign back in).*

### 2. GPU Telemetry Shows `N/A`
- **AMD Radeon GPUs**: Ensure AMD Software: Adrenalin Edition drivers are installed (`amdadlx64.dll` is discovered automatically).
- **NVIDIA / Intel GPUs**: When AMD ADLX is unavailable, `perfoverlay` automatically falls back to Windows Performance Data Helper (PDH) GPU engine counters to report overall GPU utilization.

### 3. CPU Temperature Shows `N/A`
- Open [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases).
- Navigate to `Options -> Remote Web Server -> Run` (Default Port: `8085`).

---

## Building from Source

### Prerequisites
- Windows 10/11 x64
- Visual Studio 2022 or newer (with Desktop Development with C++ workload)
- CMake 3.25 or newer
- vcpkg package manager (with `VCPKG_ROOT` environment variable configured)

### Build Commands
```powershell
# 1. Clone the repository
git clone https://github.com/MosIbOad/perfoverlay.git
cd perfoverlay

# 2. Configure and build Release (Static CRT /MT single-executable)
cmake --preset windows-release
cmake --build --preset windows-release

# 3. Run unit tests (Catch2)
ctest --preset windows-release --output-on-failure
```

The standalone executable will be generated at `build/windows-release/Release/perfoverlay.exe`.

---

## License

This project is licensed under the [MIT License](LICENSE).

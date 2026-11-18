# perfoverlay - Project Rules & Guidelines

## Project Overview
Open-source, single-executable FPS and hardware monitoring tool for Windows (MSI Afterburner + RTSS alternative).
Built with C++20 and CMake.
Displays a transparent overlay over games showing:
- FPS, 1% low, 0.1% low
- GPU and CPU usage and temperatures
- DLSS / FSR / XeSS active upscaler info

## Rules & Constraints

1. **Target Platform**:
   - Windows 10/11 x64, MSVC.
   - Windows only.

2. **Zero Injection / No Hooking (Anti-Cheat Safety)**:
   - NEVER inject DLLs or hook game processes.
   - FPS metrics captured externally via ETW (PresentMon approach).
   - Hardware telemetry gathered via official APIs.
   - Overlay is a separate, transparent, always-on-top, click-through window.

3. **GPU Telemetry (AMD Focus)**:
   - Primary GPU sensors: AMD ADLX SDK.
   - Fallback GPU sensors: Windows PDH ("GPU Engine" performance counters).

4. **Package & Dependency Management**:
   - vcpkg in manifest mode (`vcpkg.json`).

5. **Folder Structure**:
   - `src/core/`: Core metrics and logic. MUST NOT depend on Windows headers and MUST be unit-testable.
   - `src/sensors/`: Hardware monitoring (CPU, GPU via ADLX / PDH).
   - `src/capture/`: ETW / PresentMon frame capture.
   - `src/upscaler/`: Upscaler detection (DLSS, FSR, XeSS).
   - `src/ui/`: Transparent overlay UI.
   - `tests/`: Unit and integration tests.
   - `docs/`: Documentation and project rules.

6. **Modern C++ Guidelines**:
   - C++20 standard.
   - Strict RAII.
   - Modern utilities: `std::span`, `std::chrono`, `std::jthread`, smart pointers.
   - Absolutely NO raw `new` / `delete`.

7. **Development Workflow**:
   - In each step, implement ONLY the requested scope; do not add code for future steps.
   - At the end of every step:
     1. Summarize what was done.
     2. Provide build / test commands.
     3. Propose a git commit message in Conventional Commits format.

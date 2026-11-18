# Contributing to perfoverlay

Thank you for your interest in contributing to **perfoverlay**!

perfoverlay is an open-source, single-executable FPS and hardware monitoring tool for Windows, designed as an anti-cheat-safe alternative to MSI Afterburner and RivaTuner Statistics Server (RTSS).

---

## 1. Core Principles & Constraints

All contributions MUST adhere to these strict project guidelines:

1. **Zero Injection / No Hooking (Anti-Cheat Safety)**:
   - **NEVER** inject DLLs, create remote threads, or hook game processes or Direct3D swapchains.
   - FPS and frame timing must strictly be captured externally via ETW (`Microsoft-Windows-DxgKrnl` and `Microsoft-Windows-DXGI` providers).
   - The overlay is rendered in a separate, transparent, always-on-top, click-through Win32 window (`WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST`).
2. **Platform & Compiler**:
   - Windows 10/11 x64 only.
   - Microsoft Visual C++ (MSVC) with C++20 standard (`/std:c++20`, `/permissive-`, `/W4`, `/utf-8`).
   - Single-executable distribution: CRT linked statically (`/MT` in Release).
3. **Architecture & Folder Structure**:
   - `src/core/`: Pure frame time calculations, statistics, configuration, and data structures. Must remain OS-agnostic and 100% unit-testable without Windows SDK headers.
   - `src/sensors/`: Hardware monitoring (CPU via PDH, GPU via AMD ADLX SDK with PDH fallback, CPU temperature via external provider).
   - `src/capture/`: ETW PresentMon frame capture engine and active foreground game detection.
   - `src/upscaler/`: Passive file-based game directory scanner for DLSS, FSR, and XeSS libraries.
   - `src/ui/`: Direct3D 11 + Dear ImGui transparent overlay window, settings dialog, and system tray integration.
   - `tests/`: Unit and integration test suites using Catch2.
4. **Modern C++ Guidelines**:
   - Strict RAII for all resources (handles, memory, sockets, ETW sessions).
   - Use standard library types (`std::span`, `std::chrono`, `std::jthread`, `std::unique_ptr`, `std::optional`).
   - Absolutely **no raw `new` / `delete`**.

---

## 2. Setting Up Your Development Environment

### Prerequisites
- Windows 10/11 x64
- Visual Studio 2022 or newer with Desktop Development with C++ (MSVC v143+)
- CMake 3.25 or newer
- vcpkg package manager with `VCPKG_ROOT` environment variable set:
  ```powershell
  # Example: Set VCPKG_ROOT to your vcpkg clone
  [Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
  ```

### Cloning the Repository
```powershell
git clone --recursive https://github.com/berkan/perfoverlay.git
cd perfoverlay
```

### Configuring and Building
The project uses CMake Presets (`CMakePresets.json`) and vcpkg in manifest mode (`vcpkg.json`):

```powershell
# Configure Debug and Release
cmake --preset windows-debug
cmake --preset windows-release

# Build
cmake --build --preset windows-debug
cmake --build --preset windows-release
```

---

## 3. Running Unit Tests

All unit tests must pass before opening a Pull Request:

```powershell
ctest --preset windows-debug --output-on-failure
ctest --preset windows-release --output-on-failure
```

---

## 4. Code Formatting

We use `clang-format` based on the `.clang-format` configuration in the project root:

```powershell
# Check or apply formatting
clang-format -i src/**/*.cpp src/**/*.hpp tests/**/*.cpp
```

---

## 5. Commit Guidelines

We follow the [Conventional Commits](https://www.conventionalcommits.org/) specification:

- `feat:` A new feature or capability
- `fix:` A bug fix
- `docs:` Documentation improvements
- `refactor:` Code restructuring without functional change
- `test:` Adding or improving tests
- `chore:` Build scripts, dependencies, or maintenance

Examples:
- `feat(ui): add customizable font scaling slider`
- `fix(sensors): handle zero division when calculating GPU clock ratio`
- `test(config): add unit test for corrupted JSON backup recovery`

---

## 6. Submitting a Pull Request

1. Fork the repository and create a descriptive branch:
   ```powershell
   git checkout -b feat/my-new-sensor
   ```
2. Implement your changes, adhering to the coding and anti-cheat constraints.
3. Write or update Catch2 unit tests in `tests/`.
4. Ensure all presets build cleanly and all tests pass.
5. Push to your fork and submit a Pull Request describing what was changed and why.

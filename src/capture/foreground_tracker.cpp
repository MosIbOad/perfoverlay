#include "capture/foreground_tracker.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <filesystem>

namespace perfoverlay::capture {

namespace {

std::string toLower(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

struct ProcessImageInfo {
    std::string fullPath;
    std::string fileName;
};

ProcessImageInfo getProcessImageInfo(HANDLE processHandle) {
    std::array<char, MAX_PATH> pathBuffer{};
    DWORD size = static_cast<DWORD>(pathBuffer.size());
    if (QueryFullProcessImageNameA(processHandle, 0, pathBuffer.data(), &size)) {
        return ProcessImageInfo{
            .fullPath = std::string(pathBuffer.data(), size),
            .fileName = std::filesystem::path(pathBuffer.data()).filename().string()
        };
    }
    return {};
}

} // namespace

ForegroundProcessTracker::ForegroundProcessTracker()
    : ownProcessId_(GetCurrentProcessId()) {}

bool ForegroundProcessTracker::isExcluded(
    uint32_t processId,
    uint32_t ownProcessId,
    std::string_view processName,
    std::string_view windowTitle
) noexcept {
    if (processId == 0 || processId == 4) {
        return true;
    }

    if (ownProcessId != 0 && processId == ownProcessId) {
        return true;
    }

    const std::string lowerName = toLower(processName);
    if (lowerName == "explorer.exe" ||
        lowerName == "dwm.exe" ||
        lowerName == "shellexperiencehost.exe" ||
        lowerName == "startmenuexperiencehost.exe" ||
        lowerName == "searchhost.exe" ||
        lowerName == "textinputhost.exe" ||
        lowerName == "applicationframehost.exe" ||
        lowerName == "lockapp.exe") {
        return true;
    }

    if (windowTitle.empty() ||
        windowTitle == "Program Manager" ||
        windowTitle == "Taskbar" ||
        windowTitle == "Windows Shell Experience Host") {
        return true;
    }

    return false;
}

std::optional<ForegroundProcessInfo> ForegroundProcessTracker::getActiveProcess() const {
    const HWND hwnd = GetForegroundWindow();
    if (!hwnd || hwnd == GetDesktopWindow() || hwnd == GetShellWindow()) {
        return std::nullopt;
    }

    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId == 0) {
        return std::nullopt;
    }

    std::array<char, 512> titleBuffer{};
    GetWindowTextA(hwnd, titleBuffer.data(), static_cast<int>(titleBuffer.size()));
    const std::string windowTitle(titleBuffer.data());

    std::string processName;
    std::string executablePath;
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess) {
        const auto imageInfo = getProcessImageInfo(hProcess);
        processName = imageInfo.fileName;
        executablePath = imageInfo.fullPath;
        CloseHandle(hProcess);
    }

    if (isExcluded(processId, ownProcessId_, processName, windowTitle)) {
        return std::nullopt;
    }

    return ForegroundProcessInfo{
        .processId = processId,
        .processName = std::move(processName),
        .windowTitle = windowTitle,
        .executablePath = std::move(executablePath),
    };
}

} // namespace perfoverlay::capture

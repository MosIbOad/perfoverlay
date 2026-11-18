#include "upscaler/process_path.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <array>

namespace perfoverlay::upscaler {

std::string getProcessExecutablePath(uint32_t processId) noexcept {
    if (processId == 0 || processId == 4) {
        return {};
    }

    // Explicitly request ONLY PROCESS_QUERY_LIMITED_INFORMATION (Anti-cheat safe)
    // NEVER request PROCESS_VM_READ
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!hProcess) {
        return {};
    }

    std::array<char, MAX_PATH> pathBuffer{};
    DWORD size = static_cast<DWORD>(pathBuffer.size());
    std::string result;

    if (QueryFullProcessImageNameA(hProcess, 0, pathBuffer.data(), &size)) {
        result.assign(pathBuffer.data(), size);
    }

    CloseHandle(hProcess);
    return result;
}

}  // namespace perfoverlay::upscaler

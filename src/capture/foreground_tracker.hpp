#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace perfoverlay::capture {

struct ForegroundProcessInfo {
    uint32_t processId{0};
    std::string processName;
    std::string windowTitle;
};

/**
 * @brief Tracks the active foreground window and process ID using Win32 APIs
 *        (GetForegroundWindow, GetWindowThreadProcessId).
 *
 * Automatically filters out perfoverlay itself, Windows Explorer, the desktop shell,
 * and background OS system windows.
 */
class ForegroundProcessTracker {
public:
    ForegroundProcessTracker();
    ~ForegroundProcessTracker() = default;

    /**
     * @brief Queries the currently active foreground process.
     * @return Process info if an active game/application is detected, std::nullopt if
     *         no foreground window or an excluded system process is active.
     */
    [[nodiscard]] std::optional<ForegroundProcessInfo> getActiveProcess() const;

    /**
     * @brief Pure helper to evaluate if a process should be excluded from overlay tracking.
     *
     * Pure function: unit testable without active Win32 UI.
     */
    [[nodiscard]] static bool isExcluded(
        uint32_t processId,
        uint32_t ownProcessId,
        std::string_view processName,
        std::string_view windowTitle
    ) noexcept;

private:
    uint32_t ownProcessId_{0};
};

} // namespace perfoverlay::capture

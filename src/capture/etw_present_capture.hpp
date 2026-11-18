#pragma once

#include "core/frame_stats.hpp"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace perfoverlay::capture {

enum class EtwStartStatus {
    Success,
    AlreadyRunning,
    AccessDenied,            // Needs Administrator or 'Performance Log Users' membership
    SessionStartFailed,
    TraceOpenFailed
};

struct EtwStartResult {
    EtwStartStatus status{EtwStartStatus::Success};
    std::string errorMessage;

    [[nodiscard]] bool ok() const noexcept {
        return status == EtwStartStatus::Success || status == EtwStartStatus::AlreadyRunning;
    }
};

/**
 * @brief Real-time ETW session that captures graphics present events from
 *        Microsoft-Windows-DXGI and Microsoft-Windows-DxgKrnl providers (PresentMon approach).
 *
 * Runs in a dedicated background std::jthread. Zero DLL injection or game process hooking.
 * Records frame times per process into thread-safe FrameTimeBuffers.
 */
class EtwPresentCapture {
public:
    explicit EtwPresentCapture(
        size_t bufferCapacity = 2000,
        std::chrono::nanoseconds bufferDuration = std::chrono::seconds(10)
    );
    ~EtwPresentCapture();

    EtwPresentCapture(const EtwPresentCapture&) = delete;
    EtwPresentCapture& operator=(const EtwPresentCapture&) = delete;
    EtwPresentCapture(EtwPresentCapture&&) = delete;
    EtwPresentCapture& operator=(EtwPresentCapture&&) = delete;

    /**
     * @brief Starts the ETW real-time session and processing loop.
     * @return Result indicating success or actionable permission / setup error.
     */
    EtwStartResult start();

    /**
     * @brief Stops the ETW session, cleans up kernel session, and joins worker thread.
     */
    void stop();

    [[nodiscard]] bool isRunning() const noexcept;

    /**
     * @brief Computes frame stats (FPS, 1% low, 0.1% low, avg frame time) for a target PID.
     */
    [[nodiscard]] std::optional<core::FrameStats> getProcessStats(uint32_t processId) const;

    /**
     * @brief Retrieves a copy of the FrameTimeBuffer for a given process.
     */
    [[nodiscard]] std::optional<core::FrameTimeBuffer> getProcessBuffer(uint32_t processId) const;

    /**
     * @brief Returns a list of all currently tracked process IDs.
     */
    [[nodiscard]] std::vector<uint32_t> getTrackedProcessIds() const;

    /**
     * @brief Cleans up memory buffers for processes that haven't emitted frames recently.
     */
    void pruneInactiveProcesses(std::chrono::nanoseconds maxIdleTime = std::chrono::seconds(30));

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace perfoverlay::capture

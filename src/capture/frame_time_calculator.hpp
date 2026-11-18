#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace perfoverlay::capture {

/**
 * @brief Pure calculation engine that tracks consecutive present events per process
 *        and computes frame times in milliseconds from QPC timestamps.
 *
 * Completely independent of Windows ETW APIs and easily unit testable.
 * Filters out negative deltas, duplicate timestamps, and nested duplicate present calls.
 */
class FrameTimeCalculator {
public:
    /**
     * @param qpcFrequency QueryPerformanceCounter frequency (ticks per second).
     * @param minDeltaMs Minimum valid frame time in ms. Consecutive events faster than
     *                   this are considered nested duplicates and ignored.
     */
    explicit FrameTimeCalculator(uint64_t qpcFrequency, double minDeltaMs = 0.5);

    /**
     * @brief Processes a present event for a given process.
     * @param processId Target PID.
     * @param qpcTimestamp Timestamp in QPC ticks.
     * @return Frame time in milliseconds, or std::nullopt if this is the first
     *         event or an ignored duplicate.
     */
    std::optional<double> processEvent(uint32_t processId, uint64_t qpcTimestamp);

    void reset(uint32_t processId);
    void clear();

    [[nodiscard]] uint64_t qpcFrequency() const noexcept { return qpcFrequency_; }
    [[nodiscard]] double minDeltaMs() const noexcept { return minDeltaMs_; }
    [[nodiscard]] size_t trackedProcessCount() const noexcept { return lastTimestamps_.size(); }

private:
    uint64_t qpcFrequency_{1};
    double minDeltaMs_{0.1};
    std::unordered_map<uint32_t, uint64_t> lastTimestamps_;
};

}  // namespace perfoverlay::capture

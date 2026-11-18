#pragma once

#include <chrono>
#include <cstddef>
#include <span>
#include <vector>

namespace perfoverlay::core {

struct FrameEntry {
    double frameTimeMs{0.0};
    std::chrono::steady_clock::time_point timestamp{};
};

/**
 * @brief Fixed-capacity circular buffer storing frame times (in milliseconds)
 *        along with their timestamps.
 *
 * Guarantees zero heap allocation on the hot path (push).
 * Automatically discards entries that are older than maxDuration or when
 * the buffer reaches maxCapacity.
 */
class FrameTimeBuffer {
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Duration = std::chrono::nanoseconds;

    explicit FrameTimeBuffer(size_t maxCapacity = 2000,
                             Duration maxDuration = std::chrono::seconds(10));

    // Hot-path push using steady_clock::now()
    void push(double frameTimeMs);

    // Push with explicit timestamp (useful for simulation and deterministic testing)
    void push(double frameTimeMs, TimePoint timestamp);

    // Prune entries older than cutoff
    void discardOlderThan(TimePoint cutoff);

    // Prune entries older than maxAge relative to currentTime
    void discardOlderThan(Duration maxAge, TimePoint currentTime);

    // Clears all entries in the buffer
    void clear() noexcept;

    // Element access (0 = oldest, size() - 1 = newest)
    [[nodiscard]] double operator[](size_t index) const;
    [[nodiscard]] const FrameEntry& entryAt(size_t index) const;

    [[nodiscard]] double front() const; // Oldest frame time
    [[nodiscard]] double back() const;  // Most recent frame time

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] Duration maxDuration() const noexcept { return maxDuration_; }

    void setMaxDuration(Duration duration) noexcept { maxDuration_ = duration; }

    // Copies all active frame times in chronological order into a span (zero-allocation)
    void copyFrameTimes(std::span<double> out) const;

    // Copies all active frame times into a resized vector
    void copyFrameTimes(std::vector<double>& out) const;

private:
    std::vector<FrameEntry> buffer_;
    size_t capacity_{0};
    size_t head_{0}; // Next write position
    size_t tail_{0}; // Oldest active entry
    size_t size_{0};
    Duration maxDuration_{std::chrono::seconds(10)};

    void pruneOldEntries(TimePoint currentTime);
};

/**
 * @brief Aggregated frame rate and frame time statistics.
 */
struct FrameStats {
    double avgFps{0.0};
    double currentFps{0.0};
    double minFps{0.0};
    double maxFps{0.0};
    double p1LowFps{0.0};        // 1% Low FPS (average frame time of worst 1% frames)
    double p01LowFps{0.0};       // 0.1% Low FPS (average frame time of worst 0.1% frames)
    double avgFrameTimeMs{0.0};
    double p99FrameTimeMs{0.0};  // 99th percentile frame time
};

/**
 * @brief Computes frame rate and frame time statistics from a FrameTimeBuffer.
 *
 * 1% Low FPS Definition & Methodology:
 * ------------------------------------
 * There are two common approaches to calculate "1% Low":
 *
 * 1. Quantile / Percentile Threshold:
 *    Finding the single 99th percentile frame time cutoff (T_99) and computing
 *    1000 / T_99. While simple, this method only captures the boundary between
 *    the top 99% and bottom 1%. It is completely blind to how severe the stutters
 *    actually were beyond that cutoff (e.g. a 25ms hiccup vs. a 500ms freeze produce
 *    the exact same 99th percentile threshold).
 *
 * 2. Average of the Worst 1% (Expected Shortfall / 1% Average Low - Implemented Here):
 *    All frame times are sorted, and the slowest ceil(N * 0.01) frames are selected.
 *    We calculate the arithmetic mean of these worst frame times, and convert that
 *    average to FPS: p1LowFps = 1000.0 / worst1PctAverageMs.
 *    This methodology (popularized by Gamers Nexus, Digital Foundry, and PresentMon)
 *    directly captures the duration and severity of stutter spikes, accurately
 *    measuring the player's perceived smoothness.
 *
 * @param buffer Input frame time buffer.
 * @param scratch Optional pre-allocated scratch vector to avoid heap allocations
 *                on the hot path.
 * @return FrameStats structure populated with calculated metrics.
 */
FrameStats computeStats(const FrameTimeBuffer& buffer, std::vector<double>& scratch);
FrameStats computeStats(const FrameTimeBuffer& buffer);

} // namespace perfoverlay::core

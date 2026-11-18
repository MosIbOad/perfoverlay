#pragma once

#include "core/frame_stats.hpp"
#include "sensors/sensor_types.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace perfoverlay::core {

/**
 * @brief Information about the currently focused game or application.
 */
struct ActiveGameInfo {
    uint32_t processId{0};
    std::string processName;
    std::string windowTitle;
};

/**
 * @brief Unified telemetry snapshot consumed by the overlay UI and logging systems.
 */
struct MetricsSnapshot {
    bool hasActiveGame{false};
    std::optional<ActiveGameInfo> activeGame;

    // Frame rate and frame timing statistics for the active game
    std::optional<FrameStats> frameStats;

    // Hardware telemetry (CPU/GPU load, temps, VRAM, clock, power)
    sensors::SensorSnapshot sensors;

    // Timestamp when this snapshot was captured/updated
    std::chrono::steady_clock::time_point timestamp{};
};

// Abstract interface for active foreground process detection
class IForegroundProcessProvider {
public:
    virtual ~IForegroundProcessProvider() = default;
    virtual std::optional<ActiveGameInfo> getActiveProcess() const = 0;
};

// Abstract interface for frame timing data collection
class IPresentCaptureProvider {
public:
    virtual ~IPresentCaptureProvider() = default;
    virtual std::optional<FrameStats> getProcessStats(uint32_t processId) const = 0;
};

// Abstract interface for hardware sensor telemetry
class ISensorSnapshotProvider {
public:
    virtual ~ISensorSnapshotProvider() = default;
    virtual sensors::SensorSnapshot getSnapshot() const = 0;
};

/**
 * @brief Central aggregator coordinating capture, sensors, and frame timing metrics.
 *
 * Runs a background thread updating:
 * - FPS and active game stats 4 times per second (every 250 ms).
 * - Hardware sensors every 500 ms.
 *
 * Provides thread-safe, non-blocking snapshot reads for the UI thread.
 */
class MetricsHub {
public:
    MetricsHub(
        const IForegroundProcessProvider* fgProvider = nullptr,
        const IPresentCaptureProvider* captureProvider = nullptr,
        const ISensorSnapshotProvider* sensorProvider = nullptr
    );
    ~MetricsHub();

    MetricsHub(const MetricsHub&) = delete;
    MetricsHub& operator=(const MetricsHub&) = delete;
    MetricsHub(MetricsHub&&) = delete;
    MetricsHub& operator=(MetricsHub&&) = delete;

    void setForegroundProvider(const IForegroundProcessProvider* provider) noexcept {
        fgProvider_ = provider;
    }
    void setCaptureProvider(const IPresentCaptureProvider* provider) noexcept {
        captureProvider_ = provider;
    }
    void setSensorProvider(const ISensorSnapshotProvider* provider) noexcept {
        sensorProvider_ = provider;
    }

    /**
     * @brief Starts the background aggregation loop.
     */
    void start();

    /**
     * @brief Stops the background aggregation loop.
     */
    void stop();

    /**
     * @brief Performs a single synchronous update cycle (useful for tests and manual ticks).
     * @param pollSensors Whether to poll hardware sensors on this cycle.
     */
    void pollOnce(bool pollSensors = true);

    /**
     * @brief Thread-safe non-blocking retrieval of the latest snapshot.
     */
    [[nodiscard]] MetricsSnapshot getSnapshot() const;

    [[nodiscard]] bool isRunning() const noexcept { return running_.load(); }

private:
    const IForegroundProcessProvider* fgProvider_{nullptr};
    const IPresentCaptureProvider* captureProvider_{nullptr};
    const ISensorSnapshotProvider* sensorProvider_{nullptr};

    mutable std::mutex snapshotMutex_;
    MetricsSnapshot snapshot_;

    std::atomic<bool> running_{false};
    std::mutex cvMutex_;
    std::condition_variable cv_;
    std::jthread worker_;
};

} // namespace perfoverlay::core

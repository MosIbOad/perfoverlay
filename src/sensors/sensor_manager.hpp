#pragma once

#include "core/metrics_hub.hpp"
#include "sensors/sensor_types.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace perfoverlay::sensors {

/**
 * @brief Manages hardware sensors, orchestrates background polling at 500 ms intervals
 *        using a dedicated std::jthread, and provides thread-safe access to SensorSnapshot.
 */
class SensorManager : public core::ISensorSnapshotProvider {
public:
    SensorManager();
    explicit SensorManager(std::vector<std::unique_ptr<ISensor>> sensors);
    ~SensorManager();

    SensorManager(const SensorManager&) = delete;
    SensorManager& operator=(const SensorManager&) = delete;
    SensorManager(SensorManager&&) = delete;
    SensorManager& operator=(SensorManager&&) = delete;

    /**
     * @brief Adds a custom or mock sensor.
     */
    void addSensor(std::unique_ptr<ISensor> sensor);

    /**
     * @brief Initializes registered sensors (or configures defaults if none were added).
     * @return true if at least one sensor initialized successfully.
     */
    bool init();

    /**
     * @brief Starts the background polling thread (500 ms cycle).
     */
    void start();

    /**
     * @brief Stops the background polling thread.
     */
    void stop();

    /**
     * @brief Performs a single synchronous poll cycle across all active sensors.
     */
    void pollOnce();

    /**
     * @brief Returns a thread-safe copy of the latest sensor snapshot.
     */
    [[nodiscard]] SensorSnapshot getSnapshot() const override;

    [[nodiscard]] bool isRunning() const noexcept { return running_.load(); }
    [[nodiscard]] size_t sensorCount() const noexcept { return sensors_.size(); }

private:
    std::vector<std::unique_ptr<ISensor>> sensors_;
    mutable std::mutex snapshotMutex_;
    SensorSnapshot snapshot_;

    std::atomic<bool> running_{false};
    std::mutex cvMutex_;
    std::condition_variable cv_;
    std::jthread worker_;
};

} // namespace perfoverlay::sensors

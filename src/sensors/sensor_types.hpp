#pragma once

#include <optional>

namespace perfoverlay::sensors {

/**
 * @brief Snapshot of hardware metrics collected across all active sensors.
 *
 * All fields are std::optional because particular metrics may not be supported
 * or available on every hardware/driver configuration.
 */
struct SensorSnapshot {
    std::optional<double> cpuUsagePercent; // CPU utilization percentage (0.0 - 100.0)
    std::optional<double> gpuUsagePercent; // GPU core utilization percentage (0.0 - 100.0)
    std::optional<double> gpuTempC;        // GPU core temperature in degrees Celsius
    std::optional<double> gpuVramUsedMB;   // GPU dedicated video memory used (in MB)
    std::optional<double> gpuVramTotalMB;  // GPU dedicated video memory total capacity (in MB)
    std::optional<double> gpuClockMHz;     // GPU core clock speed in MHz
    std::optional<double> gpuPowerW;       // GPU power consumption in Watts
};

/**
 * @brief Common interface for hardware telemetry sensors.
 */
class ISensor {
public:
    virtual ~ISensor() = default;

    /**
     * @brief Initializes the sensor (e.g. loads DLLs, opens handles/queries).
     * @return true if initialized successfully and ready to poll, false otherwise.
     */
    virtual bool init() = 0;

    /**
     * @brief Polls the latest sensor data. Must be non-blocking.
     */
    virtual void poll() = 0;

    /**
     * @brief Merges this sensor's latest readings into the target snapshot.
     */
    virtual void updateSnapshot(SensorSnapshot& snapshot) const = 0;
};

} // namespace perfoverlay::sensors

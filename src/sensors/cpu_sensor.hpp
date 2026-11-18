#pragma once

#include "sensors/sensor_types.hpp"
#include <cstdint>

namespace perfoverlay::sensors {

/**
 * @brief CPU utilization sensor using non-blocking Windows GetSystemTimes API.
 *
 * Measures kernel, user, and idle time deltas on each poll() invocation
 * without blocking the calling thread.
 */
class CpuSensor : public ISensor {
public:
    CpuSensor();
    ~CpuSensor() override = default;

    bool init() override;
    void poll() override;
    void updateSnapshot(SensorSnapshot& snapshot) const override;

    [[nodiscard]] std::optional<double> getCpuUsagePercent() const noexcept {
        return cpuUsagePercent_;
    }

private:
    uint64_t prevIdleTime_{0};
    uint64_t prevKernelTime_{0};
    uint64_t prevUserTime_{0};
    bool initialized_{false};
    std::optional<double> cpuUsagePercent_;
};

} // namespace perfoverlay::sensors

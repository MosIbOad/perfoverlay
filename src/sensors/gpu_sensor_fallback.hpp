#pragma once

#include "sensors/sensor_types.hpp"
#include <memory>

namespace perfoverlay::sensors {

/**
 * @brief Fallback GPU sensor using Windows Performance Data Helper (PDH)
 *        counters ("\\GPU Engine(*)\\Utilization Percentage").
 *
 * Used when vendor-specific SDKs (such as ADLX) are not available or fail.
 * Gathers overall GPU utilization percentage across active GPU engines.
 */
class GpuSensorFallback : public ISensor {
public:
    GpuSensorFallback();
    ~GpuSensorFallback() override;

    bool init() override;
    void poll() override;
    void updateSnapshot(SensorSnapshot& snapshot) const override;

    [[nodiscard]] bool isInitialized() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace perfoverlay::sensors

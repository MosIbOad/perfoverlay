#pragma once

#include "sensors/sensor_types.hpp"
#include <memory>

namespace perfoverlay::sensors {

/**
 * @brief AMD GPU telemetry sensor using the official AMD ADLX SDK.
 *
 * Dynamically loads amdadlx64.dll at runtime. If the AMD driver or DLL is not
 * present, init() safely returns false without crashing.
 *
 * Telemetry gathered:
 * - GPU utilization (%)
 * - GPU temperature (°C)
 * - VRAM used and total (MB)
 * - GPU clock frequency (MHz)
 * - GPU power consumption (W)
 */
class AmdGpuSensor : public ISensor {
public:
    AmdGpuSensor();
    ~AmdGpuSensor() override;

    bool init() override;
    void poll() override;
    void updateSnapshot(SensorSnapshot& snapshot) const override;

    [[nodiscard]] bool isInitialized() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace perfoverlay::sensors

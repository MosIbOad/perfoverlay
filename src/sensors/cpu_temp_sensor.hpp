#pragma once

#include "sensors/cpu_temp_provider.hpp"
#include "sensors/sensor_types.hpp"

#include <memory>
#include <optional>

namespace perfoverlay::sensors {

/**
 * @brief Hardware sensor wrapping ICpuTempProvider and updating SensorSnapshot::cpuTempC.
 *
 * Implements ISensor. Polls the selected provider (e.g. LibreHardwareMonitor HTTP)
 * without blocking or locking the main thread.
 */
class CpuTempSensor : public ISensor {
public:
    explicit CpuTempSensor(std::unique_ptr<ICpuTempProvider> provider = nullptr);
    ~CpuTempSensor() override = default;

    bool init() override;
    void poll() override;
    void updateSnapshot(SensorSnapshot& snapshot) const override;

    [[nodiscard]] std::optional<double> getCurrentTempC() const noexcept { return currentTempC_; }

private:
    std::unique_ptr<ICpuTempProvider> provider_;
    std::optional<double> currentTempC_;
};

}  // namespace perfoverlay::sensors

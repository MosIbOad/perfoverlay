#include "sensors/cpu_temp_sensor.hpp"

namespace perfoverlay::sensors {

CpuTempSensor::CpuTempSensor(std::unique_ptr<ICpuTempProvider> provider)
    : provider_(std::move(provider)) {}

bool CpuTempSensor::init() {
    if (!provider_) {
        provider_ = createCpuTempProvider(CpuTempProviderType::Auto);
    }
    return true;
}

void CpuTempSensor::poll() {
    if (provider_) {
        currentTempC_ = provider_->getCpuTempC();
    } else {
        currentTempC_ = std::nullopt;
    }
}

void CpuTempSensor::updateSnapshot(SensorSnapshot& snapshot) const {
    snapshot.cpuTempC = currentTempC_;
}

} // namespace perfoverlay::sensors

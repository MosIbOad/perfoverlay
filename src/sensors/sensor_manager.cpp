#include "sensors/sensor_manager.hpp"
#include "sensors/cpu_sensor.hpp"
#include "sensors/cpu_temp_sensor.hpp"
#include "sensors/amd_gpu_sensor.hpp"
#include "sensors/gpu_sensor_fallback.hpp"

#include <chrono>

namespace perfoverlay::sensors {

SensorManager::SensorManager() = default;

SensorManager::SensorManager(std::vector<std::unique_ptr<ISensor>> sensors)
    : sensors_(std::move(sensors)) {}

SensorManager::~SensorManager() {
    stop();
}

void SensorManager::addSensor(std::unique_ptr<ISensor> sensor) {
    if (sensor) {
        sensors_.push_back(std::move(sensor));
    }
}

bool SensorManager::init() {
    if (sensors_.empty()) {
        // 1. CPU Sensor
        auto cpu = std::make_unique<CpuSensor>();
        if (cpu->init()) {
            sensors_.push_back(std::move(cpu));
        }

        // 2. CPU Temperature Sensor (Optional external provider, no kernel driver)
        auto cpuTemp = std::make_unique<CpuTempSensor>();
        if (cpuTemp->init()) {
            sensors_.push_back(std::move(cpuTemp));
        }

        // 3. GPU Sensor: AMD ADLX first, fallback to PDH GPU Engine
        auto amdGpu = std::make_unique<AmdGpuSensor>();
        if (amdGpu->init()) {
            sensors_.push_back(std::move(amdGpu));
        } else {
            auto fallbackGpu = std::make_unique<GpuSensorFallback>();
            if (fallbackGpu->init()) {
                sensors_.push_back(std::move(fallbackGpu));
            }
        }
    } else {
        // Initialize user-provided sensors
        for (auto& sensor : sensors_) {
            sensor->init();
        }
    }

    return !sensors_.empty();
}

void SensorManager::start() {
    if (running_.exchange(true)) {
        return;
    }

    worker_ = std::jthread([this](std::stop_token stopToken) {
        while (!stopToken.stop_requested()) {
            pollOnce();

            std::unique_lock lock(cvMutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(500), [&stopToken] {
                return stopToken.stop_requested();
            });
        }
    });
}

void SensorManager::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (worker_.joinable()) {
        worker_.request_stop();
        cv_.notify_all();
        worker_.join();
    }
}

void SensorManager::pollOnce() {
    SensorSnapshot current;
    for (auto& sensor : sensors_) {
        sensor->poll();
        sensor->updateSnapshot(current);
    }

    {
        std::lock_guard lock(snapshotMutex_);
        snapshot_ = current;
    }
}

SensorSnapshot SensorManager::getSnapshot() const {
    std::lock_guard lock(snapshotMutex_);
    return snapshot_;
}

} // namespace perfoverlay::sensors

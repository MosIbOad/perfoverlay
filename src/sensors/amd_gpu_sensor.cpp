#include "sensors/amd_gpu_sensor.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Disable MSVC warnings inside third-party ADLX headers
#pragma warning(push)
#pragma warning(disable: 4100 4201 4267 4505)
#include "ADLXHelper.h"
#include "Include/IPerformanceMonitoring.h"
#pragma warning(pop)

namespace perfoverlay::sensors {

class AmdGpuSensor::Impl {
public:
    Impl() = default;

    ~Impl() {
        shutdown();
    }

    bool init() {
        // Probe whether amdadlx64.dll is present to avoid popups on non-AMD systems
        HMODULE probe = LoadLibraryExA(
            "amdadlx64.dll",
            nullptr,
            LOAD_LIBRARY_SEARCH_USER_DIRS |
            LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
            LOAD_LIBRARY_SEARCH_DEFAULT_DIRS |
            LOAD_LIBRARY_SEARCH_SYSTEM32
        );
        if (!probe) {
            return false;
        }
        FreeLibrary(probe);

        ADLX_RESULT res = helper_.Initialize();
        if (!ADLX_SUCCEEDED(res)) {
            return false;
        }

        adlx::IADLXSystem* sys = helper_.GetSystemServices();
        if (!sys) {
            shutdown();
            return false;
        }

        res = sys->GetPerformanceMonitoringServices(&perfServices_);
        if (!ADLX_SUCCEEDED(res) || !perfServices_) {
            shutdown();
            return false;
        }

        adlx::IADLXGPUListPtr gpus;
        res = sys->GetGPUs(&gpus);
        if (!ADLX_SUCCEEDED(res) || !gpus || gpus->Size() == 0) {
            shutdown();
            return false;
        }

        res = gpus->At(0, &gpu_);
        if (!ADLX_SUCCEEDED(res) || !gpu_) {
            shutdown();
            return false;
        }

        // Query total dedicated VRAM once
        adlx_uint vramMB = 0;
        if (ADLX_SUCCEEDED(gpu_->TotalVRAM(&vramMB))) {
            vramTotalMB_ = static_cast<double>(vramMB);
        }

        initialized_ = true;
        return true;
    }

    void poll() {
        if (!initialized_ || !perfServices_ || !gpu_) {
            return;
        }

        adlx::IADLXGPUMetricsPtr metrics;
        const ADLX_RESULT res = perfServices_->GetCurrentGPUMetrics(gpu_, &metrics);
        if (!ADLX_SUCCEEDED(res) || !metrics) {
            return;
        }

        // 1. GPU Utilization (%)
        adlx_double usage = 0.0;
        if (ADLX_SUCCEEDED(metrics->GPUUsage(&usage))) {
            gpuUsagePercent_ = usage;
        }

        // 2. GPU Temperature (°C)
        adlx_double temp = 0.0;
        if (ADLX_SUCCEEDED(metrics->GPUTemperature(&temp))) {
            gpuTempC_ = temp;
        }

        // 3. GPU VRAM used (MB)
        adlx_int vramUsed = 0;
        if (ADLX_SUCCEEDED(metrics->GPUVRAM(&vramUsed))) {
            gpuVramUsedMB_ = static_cast<double>(vramUsed);
        }

        // 4. GPU Clock frequency (MHz)
        adlx_int clock = 0;
        if (ADLX_SUCCEEDED(metrics->GPUClockSpeed(&clock))) {
            gpuClockMHz_ = static_cast<double>(clock);
        }

        // 5. GPU Power (Watts)
        adlx_double power = 0.0;
        if (ADLX_SUCCEEDED(metrics->GPUPower(&power))) {
            gpuPowerW_ = power;
        } else if (ADLX_SUCCEEDED(metrics->GPUTotalBoardPower(&power))) {
            gpuPowerW_ = power;
        }
    }

    void updateSnapshot(SensorSnapshot& snapshot) const {
        if (gpuUsagePercent_.has_value()) {
            snapshot.gpuUsagePercent = gpuUsagePercent_;
        }
        if (gpuTempC_.has_value()) {
            snapshot.gpuTempC = gpuTempC_;
        }
        if (gpuVramUsedMB_.has_value()) {
            snapshot.gpuVramUsedMB = gpuVramUsedMB_;
        }
        if (vramTotalMB_.has_value()) {
            snapshot.gpuVramTotalMB = vramTotalMB_;
        }
        if (gpuClockMHz_.has_value()) {
            snapshot.gpuClockMHz = gpuClockMHz_;
        }
        if (gpuPowerW_.has_value()) {
            snapshot.gpuPowerW = gpuPowerW_;
        }
    }

    [[nodiscard]] bool isInitialized() const noexcept {
        return initialized_;
    }

private:
    void shutdown() {
        // Critical: Release ADLX interface pointers before terminating helper
        perfServices_ = nullptr;
        gpu_ = nullptr;
        if (initialized_) {
            helper_.Terminate();
            initialized_ = false;
        }
    }

    ADLXHelper helper_;
    adlx::IADLXPerformanceMonitoringServicesPtr perfServices_;
    adlx::IADLXGPUPtr gpu_;
    bool initialized_{false};

    std::optional<double> gpuUsagePercent_;
    std::optional<double> gpuTempC_;
    std::optional<double> gpuVramUsedMB_;
    std::optional<double> vramTotalMB_;
    std::optional<double> gpuClockMHz_;
    std::optional<double> gpuPowerW_;
};

AmdGpuSensor::AmdGpuSensor()
    : impl_(std::make_unique<Impl>()) {}

AmdGpuSensor::~AmdGpuSensor() = default;

bool AmdGpuSensor::init() {
    return impl_->init();
}

void AmdGpuSensor::poll() {
    impl_->poll();
}

void AmdGpuSensor::updateSnapshot(SensorSnapshot& snapshot) const {
    impl_->updateSnapshot(snapshot);
}

bool AmdGpuSensor::isInitialized() const noexcept {
    return impl_->isInitialized();
}

} // namespace perfoverlay::sensors

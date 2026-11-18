#include "sensors/gpu_sensor_fallback.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <pdh.h>
#include <pdhmsg.h>
#include <windows.h>

#include <algorithm>
#include <vector>

namespace perfoverlay::sensors {

class GpuSensorFallback::Impl {
public:
    Impl() = default;

    ~Impl() { shutdown(); }

    bool init() {
        PDH_STATUS status = PdhOpenQueryW(nullptr, 0, &query_);
        if (status != ERROR_SUCCESS) {
            return false;
        }

        // Add wildcard GPU Engine utilization counter
        status = PdhAddEnglishCounterW(query_, L"\\GPU Engine(*)\\Utilization Percentage", 0, &counter_);
        if (status != ERROR_SUCCESS) {
            shutdown();
            return false;
        }

        // Perform first collection to establish baseline
        PdhCollectQueryData(query_);
        initialized_ = true;
        return true;
    }

    void poll() {
        if (!initialized_ || !query_) {
            return;
        }

        PDH_STATUS status = PdhCollectQueryData(query_);
        if (status != ERROR_SUCCESS) {
            return;
        }

        DWORD bufferSize = 0;
        DWORD itemCount = 0;
        status = PdhGetFormattedCounterArrayW(counter_, PDH_FMT_DOUBLE, &bufferSize, &itemCount, nullptr);
        if (status != PDH_MORE_DATA || bufferSize == 0) {
            return;
        }

        std::vector<BYTE> buffer(bufferSize);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        status = PdhGetFormattedCounterArrayW(counter_, PDH_FMT_DOUBLE, &bufferSize, &itemCount, items);
        if (status != ERROR_SUCCESS) {
            return;
        }

        double totalUsage = 0.0;
        for (DWORD i = 0; i < itemCount; ++i) {
            if (items[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA) {
                totalUsage += items[i].FmtValue.doubleValue;
            }
        }
        gpuUsagePercent_ = std::clamp(totalUsage, 0.0, 100.0);
    }

    void updateSnapshot(SensorSnapshot& snapshot) const {
        if (gpuUsagePercent_.has_value()) {
            snapshot.gpuUsagePercent = gpuUsagePercent_;
        }
    }

    [[nodiscard]] bool isInitialized() const noexcept { return initialized_; }

private:
    void shutdown() {
        if (query_) {
            PdhCloseQuery(query_);
            query_ = nullptr;
            counter_ = nullptr;
        }
        initialized_ = false;
    }

    PDH_HQUERY query_{nullptr};
    PDH_HCOUNTER counter_{nullptr};
    bool initialized_{false};
    std::optional<double> gpuUsagePercent_;
};

GpuSensorFallback::GpuSensorFallback() : impl_(std::make_unique<Impl>()) {}

GpuSensorFallback::~GpuSensorFallback() = default;

bool GpuSensorFallback::init() {
    return impl_->init();
}

void GpuSensorFallback::poll() {
    impl_->poll();
}

void GpuSensorFallback::updateSnapshot(SensorSnapshot& snapshot) const {
    impl_->updateSnapshot(snapshot);
}

bool GpuSensorFallback::isInitialized() const noexcept {
    return impl_->isInitialized();
}

}  // namespace perfoverlay::sensors

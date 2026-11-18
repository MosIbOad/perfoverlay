#include "sensors/cpu_sensor.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <algorithm>

namespace perfoverlay::sensors {

namespace {

uint64_t fileTimeToUint64(const FILETIME& ft) noexcept {
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

} // namespace

CpuSensor::CpuSensor() = default;

bool CpuSensor::init() {
    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        return false;
    }

    prevIdleTime_ = fileTimeToUint64(idleTime);
    prevKernelTime_ = fileTimeToUint64(kernelTime);
    prevUserTime_ = fileTimeToUint64(userTime);
    initialized_ = true;
    return true;
}

void CpuSensor::poll() {
    if (!initialized_) {
        return;
    }

    FILETIME idleTime, kernelTime, userTime;
    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
        return;
    }

    const uint64_t idle = fileTimeToUint64(idleTime);
    const uint64_t kernel = fileTimeToUint64(kernelTime);
    const uint64_t user = fileTimeToUint64(userTime);

    const uint64_t deltaIdle = idle - prevIdleTime_;
    const uint64_t deltaKernel = kernel - prevKernelTime_;
    const uint64_t deltaUser = user - prevUserTime_;

    prevIdleTime_ = idle;
    prevKernelTime_ = kernel;
    prevUserTime_ = user;

    // Total system time is (kernel + user). In Windows, kernel time includes idle time.
    const uint64_t totalSys = deltaKernel + deltaUser;
    if (totalSys > 0 && deltaKernel >= deltaIdle) {
        const uint64_t busyTime = totalSys - deltaIdle;
        const double percent = (static_cast<double>(busyTime) * 100.0) / static_cast<double>(totalSys);
        cpuUsagePercent_ = std::clamp(percent, 0.0, 100.0);
    } else {
        cpuUsagePercent_ = 0.0;
    }
}

void CpuSensor::updateSnapshot(SensorSnapshot& snapshot) const {
    if (cpuUsagePercent_.has_value()) {
        snapshot.cpuUsagePercent = cpuUsagePercent_;
    }
}

} // namespace perfoverlay::sensors

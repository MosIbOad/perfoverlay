#include "sensors/sensor_types.hpp"
#include "sensors/sensor_manager.hpp"
#include "sensors/cpu_sensor.hpp"
#include "sensors/amd_gpu_sensor.hpp"
#include "sensors/gpu_sensor_fallback.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <chrono>
#include <thread>

using namespace perfoverlay::sensors;

namespace {

class MockSensor : public ISensor {
public:
    bool initResult{true};
    int initCalls{0};
    int pollCalls{0};
    SensorSnapshot values;

    bool init() override {
        ++initCalls;
        return initResult;
    }

    void poll() override {
        ++pollCalls;
    }

    void updateSnapshot(SensorSnapshot& snapshot) const override {
        if (values.cpuUsagePercent.has_value()) snapshot.cpuUsagePercent = values.cpuUsagePercent;
        if (values.gpuUsagePercent.has_value()) snapshot.gpuUsagePercent = values.gpuUsagePercent;
        if (values.gpuTempC.has_value()) snapshot.gpuTempC = values.gpuTempC;
        if (values.gpuVramUsedMB.has_value()) snapshot.gpuVramUsedMB = values.gpuVramUsedMB;
        if (values.gpuVramTotalMB.has_value()) snapshot.gpuVramTotalMB = values.gpuVramTotalMB;
        if (values.gpuClockMHz.has_value()) snapshot.gpuClockMHz = values.gpuClockMHz;
        if (values.gpuPowerW.has_value()) snapshot.gpuPowerW = values.gpuPowerW;
    }
};

} // namespace

TEST_CASE("SensorSnapshot - Default values are std::nullopt", "[sensors]") {
    SensorSnapshot snapshot;
    REQUIRE_FALSE(snapshot.cpuUsagePercent.has_value());
    REQUIRE_FALSE(snapshot.gpuUsagePercent.has_value());
    REQUIRE_FALSE(snapshot.gpuTempC.has_value());
    REQUIRE_FALSE(snapshot.gpuVramUsedMB.has_value());
    REQUIRE_FALSE(snapshot.gpuVramTotalMB.has_value());
    REQUIRE_FALSE(snapshot.gpuClockMHz.has_value());
    REQUIRE_FALSE(snapshot.gpuPowerW.has_value());
}

TEST_CASE("SensorManager - Mock sensors orchestration and polling", "[sensors]") {
    auto mock1 = std::make_unique<MockSensor>();
    auto* pMock1 = mock1.get();
    pMock1->values.cpuUsagePercent = 42.5;

    auto mock2 = std::make_unique<MockSensor>();
    auto* pMock2 = mock2.get();
    pMock2->values.gpuUsagePercent = 88.0;
    pMock2->values.gpuTempC = 65.0;
    pMock2->values.gpuVramUsedMB = 4096.0;
    pMock2->values.gpuVramTotalMB = 16384.0;
    pMock2->values.gpuClockMHz = 2400.0;
    pMock2->values.gpuPowerW = 180.0;

    std::vector<std::unique_ptr<ISensor>> list;
    list.push_back(std::move(mock1));
    list.push_back(std::move(mock2));

    SensorManager manager(std::move(list));

    REQUIRE(manager.sensorCount() == 2);
    REQUIRE(manager.init());
    REQUIRE(pMock1->initCalls == 1);
    REQUIRE(pMock2->initCalls == 1);

    // Initial snapshot before poll should be empty
    SensorSnapshot initial = manager.getSnapshot();
    REQUIRE_FALSE(initial.cpuUsagePercent.has_value());

    // Single synchronous poll
    manager.pollOnce();
    REQUIRE(pMock1->pollCalls == 1);
    REQUIRE(pMock2->pollCalls == 1);

    SensorSnapshot snap = manager.getSnapshot();
    REQUIRE(snap.cpuUsagePercent.has_value());
    REQUIRE(*snap.cpuUsagePercent == 42.5);
    REQUIRE(snap.gpuUsagePercent.has_value());
    REQUIRE(*snap.gpuUsagePercent == 88.0);
    REQUIRE(snap.gpuTempC.has_value());
    REQUIRE(*snap.gpuTempC == 65.0);
    REQUIRE(snap.gpuVramUsedMB.has_value());
    REQUIRE(*snap.gpuVramUsedMB == 4096.0);
    REQUIRE(snap.gpuVramTotalMB.has_value());
    REQUIRE(*snap.gpuVramTotalMB == 16384.0);
    REQUIRE(snap.gpuClockMHz.has_value());
    REQUIRE(*snap.gpuClockMHz == 2400.0);
    REQUIRE(snap.gpuPowerW.has_value());
    REQUIRE(*snap.gpuPowerW == 180.0);
}

TEST_CASE("SensorManager - Background polling thread lifecycle (std::jthread)", "[sensors]") {
    auto mock = std::make_unique<MockSensor>();
    auto* pMock = mock.get();
    pMock->values.cpuUsagePercent = 15.0;

    SensorManager manager;
    manager.addSensor(std::move(mock));
    REQUIRE(manager.init());

    manager.start();
    REQUIRE(manager.isRunning());

    // Wait slightly more than one 500 ms poll cycle
    std::this_thread::sleep_for(std::chrono::milliseconds(600));

    // Must have polled at least once
    REQUIRE(pMock->pollCalls >= 1);
    SensorSnapshot snap = manager.getSnapshot();
    REQUIRE(snap.cpuUsagePercent.has_value());
    REQUIRE(*snap.cpuUsagePercent == 15.0);

    manager.stop();
    REQUIRE_FALSE(manager.isRunning());
}

TEST_CASE("CpuSensor - Live initialization and poll", "[sensors][live]") {
    CpuSensor cpu;
    REQUIRE(cpu.init());

    // Wait short moment to accumulate CPU time delta
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    cpu.poll();

    SensorSnapshot snapshot;
    cpu.updateSnapshot(snapshot);

    REQUIRE(snapshot.cpuUsagePercent.has_value());
    REQUIRE(*snapshot.cpuUsagePercent >= 0.0);
    REQUIRE(*snapshot.cpuUsagePercent <= 100.0);
}

TEST_CASE("AmdGpuSensor - Safe initialization without crashing", "[sensors][live]") {
    AmdGpuSensor amdSensor;
    // On systems with AMD GPUs/drivers, this returns true. On non-AMD systems,
    // it must gracefully return false without crashing or throwing.
    const bool ok = amdSensor.init();
    if (ok) {
        amdSensor.poll();
        SensorSnapshot snap;
        amdSensor.updateSnapshot(snap);
        // If AMD GPU is present, metrics should be queryable
        SUCCEED("AMD ADLX initialized successfully");
    } else {
        REQUIRE_FALSE(amdSensor.isInitialized());
        SUCCEED("AMD ADLX returned false safely on non-AMD / incompatible driver");
    }
}

TEST_CASE("GpuSensorFallback - Safe initialization without crashing", "[sensors][live]") {
    GpuSensorFallback fallback;
    const bool ok = fallback.init();
    if (ok) {
        fallback.poll();
        SensorSnapshot snap;
        fallback.updateSnapshot(snap);
        SUCCEED("PDH GPU Engine counter initialized");
    } else {
        REQUIRE_FALSE(fallback.isInitialized());
        SUCCEED("PDH GPU Engine counter returned false safely when counters unavailable");
    }
}

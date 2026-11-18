#include "core/metrics_hub.hpp"
#include "upscaler/upscaler_types.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>
#include <unordered_map>
#include <vector>

using Catch::Approx;
using namespace perfoverlay::core;
using namespace perfoverlay::sensors;
using namespace perfoverlay::upscaler;

namespace {

class MockForegroundProvider : public IForegroundProcessProvider {
public:
    std::optional<ActiveGameInfo> activeGame;
    int callCount{0};

    std::optional<ActiveGameInfo> getActiveProcess() const override {
        const_cast<MockForegroundProvider*>(this)->callCount++;
        return activeGame;
    }
};

class MockPresentCaptureProvider : public IPresentCaptureProvider {
public:
    std::unordered_map<uint32_t, FrameStats> statsMap;
    int callCount{0};

    std::optional<FrameStats> getProcessStats(uint32_t processId) const override {
        const_cast<MockPresentCaptureProvider*>(this)->callCount++;
        auto it = statsMap.find(processId);
        if (it != statsMap.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

class MockSensorSnapshotProvider : public ISensorSnapshotProvider {
public:
    SensorSnapshot currentSnapshot;
    int callCount{0};

    SensorSnapshot getSnapshot() const override {
        const_cast<MockSensorSnapshotProvider*>(this)->callCount++;
        return currentSnapshot;
    }
};

class MockUpscalerProvider : public IUpscalerProvider {
public:
    std::unordered_map<uint32_t, perfoverlay::upscaler::UpscalerInfo> upscalerMap;
    int callCount{0};

    std::optional<perfoverlay::upscaler::UpscalerInfo> getUpscalerInfo(
        uint32_t processId,
        std::string_view /*executablePath*/
    ) override {
        const_cast<MockUpscalerProvider*>(this)->callCount++;
        auto it = upscalerMap.find(processId);
        if (it != upscalerMap.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

} // namespace

TEST_CASE("MetricsHub - Null providers produce empty snapshot", "[hub]") {
    MetricsHub hub(nullptr, nullptr, nullptr);
    hub.pollOnce(true);

    const MetricsSnapshot snap = hub.getSnapshot();
    REQUIRE_FALSE(snap.hasActiveGame);
    REQUIRE_FALSE(snap.activeGame.has_value());
    REQUIRE_FALSE(snap.frameStats.has_value());
    REQUIRE_FALSE(snap.sensors.cpuUsagePercent.has_value());
}

TEST_CASE("MetricsHub - Aggregates active game, frame stats, and sensors", "[hub]") {
    MockForegroundProvider fg;
    fg.activeGame = ActiveGameInfo{
        .processId = 4242,
        .processName = "Cyberpunk2077.exe",
        .windowTitle = "Cyberpunk 2077"
    };

    MockPresentCaptureProvider capture;
    capture.statsMap[4242] = FrameStats{
        .avgFps = 120.0,
        .currentFps = 122.5,
        .minFps = 80.0,
        .maxFps = 144.0,
        .p1LowFps = 95.0,
        .p01LowFps = 75.0,
        .avgFrameTimeMs = 8.33,
        .p99FrameTimeMs = 10.5
    };

    MockSensorSnapshotProvider sensors;
    sensors.currentSnapshot.cpuUsagePercent = 28.5;
    sensors.currentSnapshot.gpuUsagePercent = 92.0;
    sensors.currentSnapshot.gpuTempC = 64.0;
    sensors.currentSnapshot.gpuPowerW = 210.0;

    MetricsHub hub(&fg, &capture, &sensors);
    hub.pollOnce(true);

    const MetricsSnapshot snap = hub.getSnapshot();

    REQUIRE(snap.hasActiveGame);
    REQUIRE(snap.activeGame.has_value());
    REQUIRE(snap.activeGame->processId == 4242);
    REQUIRE(snap.activeGame->processName == "Cyberpunk2077.exe");
    REQUIRE(snap.activeGame->windowTitle == "Cyberpunk 2077");

    REQUIRE(snap.frameStats.has_value());
    REQUIRE(snap.frameStats->avgFps == Approx(120.0));
    REQUIRE(snap.frameStats->currentFps == Approx(122.5));
    REQUIRE(snap.frameStats->p1LowFps == Approx(95.0));

    REQUIRE(snap.sensors.cpuUsagePercent.has_value());
    REQUIRE(*snap.sensors.cpuUsagePercent == Approx(28.5));
    REQUIRE(snap.sensors.gpuUsagePercent.has_value());
    REQUIRE(*snap.sensors.gpuUsagePercent == Approx(92.0));
    REQUIRE(snap.sensors.gpuTempC.has_value());
    REQUIRE(*snap.sensors.gpuTempC == Approx(64.0));
    REQUIRE(snap.sensors.gpuPowerW.has_value());
    REQUIRE(*snap.sensors.gpuPowerW == Approx(210.0));
}

TEST_CASE("MetricsHub - Clearly indicates when no active game is in foreground", "[hub]") {
    MockForegroundProvider fg;
    fg.activeGame = std::nullopt; // Desktop / Explorer / None

    MockPresentCaptureProvider capture;
    capture.statsMap[1234] = FrameStats{.avgFps = 60.0};

    MockSensorSnapshotProvider sensors;
    sensors.currentSnapshot.cpuUsagePercent = 5.0;

    MetricsHub hub(&fg, &capture, &sensors);
    hub.pollOnce(true);

    const MetricsSnapshot snap = hub.getSnapshot();

    REQUIRE_FALSE(snap.hasActiveGame);
    REQUIRE_FALSE(snap.activeGame.has_value());
    REQUIRE_FALSE(snap.frameStats.has_value()); // Frame stats not reported without active game

    // Sensors are still reported even when no game is active
    REQUIRE(snap.sensors.cpuUsagePercent.has_value());
    REQUIRE(*snap.sensors.cpuUsagePercent == Approx(5.0));
}

TEST_CASE("MetricsHub - Selective polling preserves sensors when pollSensors is false", "[hub]") {
    MockForegroundProvider fg;
    fg.activeGame = ActiveGameInfo{.processId = 100, .processName = "test.exe", .windowTitle = "Test"};

    MockPresentCaptureProvider capture;
    capture.statsMap[100] = FrameStats{.avgFps = 60.0};

    MockSensorSnapshotProvider sensors;
    sensors.currentSnapshot.cpuUsagePercent = 10.0;

    MetricsHub hub(&fg, &capture, &sensors);

    // 1. Initial poll with sensors
    hub.pollOnce(true);
    REQUIRE(sensors.callCount == 1);
    REQUIRE(*hub.getSnapshot().sensors.cpuUsagePercent == Approx(10.0));

    // Change sensor reading in mock provider
    sensors.currentSnapshot.cpuUsagePercent = 50.0;

    // 2. Poll without sensors (e.g. 250 ms FPS tick)
    hub.pollOnce(false);
    REQUIRE(sensors.callCount == 1); // Not called
    // Sensor reading from previous cycle must be preserved
    REQUIRE(*hub.getSnapshot().sensors.cpuUsagePercent == Approx(10.0));

    // 3. Poll with sensors (500 ms sensor tick)
    hub.pollOnce(true);
    REQUIRE(sensors.callCount == 2);
    REQUIRE(*hub.getSnapshot().sensors.cpuUsagePercent == Approx(50.0));
}

TEST_CASE("MetricsHub - Background thread lifecycle and concurrent reads", "[hub]") {
    MockForegroundProvider fg;
    fg.activeGame = ActiveGameInfo{.processId = 777, .processName = "game.exe", .windowTitle = "Game"};

    MockPresentCaptureProvider capture;
    capture.statsMap[777] = FrameStats{.avgFps = 144.0};

    MockSensorSnapshotProvider sensors;
    sensors.currentSnapshot.gpuUsagePercent = 99.0;

    MetricsHub hub(&fg, &capture, &sensors);
    hub.start();
    REQUIRE(hub.isRunning());

    // Spin up 4 concurrent reader threads simulating UI access
    std::atomic<bool> stopReaders{false};
    std::vector<std::jthread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&hub, &stopReaders] {
            while (!stopReaders.load()) {
                const auto s = hub.getSnapshot();
                // Ensure snapshot consistency
                if (s.hasActiveGame) {
                    REQUIRE(s.activeGame.has_value());
                }
                std::this_thread::yield();
            }
        });
    }

    // Run for 300 ms to span at least one 250 ms tick
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    stopReaders.store(true);
    readers.clear();

    hub.stop();
    REQUIRE_FALSE(hub.isRunning());
}

TEST_CASE("MetricsHub - Integrates upscaler detection for active game", "[hub]") {
    MockForegroundProvider fg;
    fg.activeGame = ActiveGameInfo{
        .processId = 7777,
        .processName = "Cyberpunk2077.exe",
        .windowTitle = "Cyberpunk 2077",
        .executablePath = "C:/Games/Cyberpunk/bin/x64/Cyberpunk2077.exe"
    };

    MockPresentCaptureProvider capture;
    MockSensorSnapshotProvider sensors;
    MockUpscalerProvider upscaler;
    upscaler.upscalerMap[7777] = perfoverlay::upscaler::UpscalerInfo{
        .dlss = true,
        .dlssFrameGen = true,
        .fsr = "FSR 3",
        .xess = false
    };

    MetricsHub hub(&fg, &capture, &sensors, &upscaler);
    hub.pollOnce(true);

    const MetricsSnapshot snap = hub.getSnapshot();
    REQUIRE(snap.hasActiveGame);
    REQUIRE(snap.upscaler.has_value());
    REQUIRE(snap.upscaler->dlss);
    REQUIRE(snap.upscaler->dlssFrameGen);
    REQUIRE(snap.upscaler->fsr == "FSR 3");
    REQUIRE_FALSE(snap.upscaler->xess);
    REQUIRE(snap.upscaler->formatSummary() == "DLSS, DLSS-FG, FSR 3");

    // When active game is lost, upscaler should be cleared
    fg.activeGame = std::nullopt;
    hub.pollOnce(true);

    const MetricsSnapshot emptySnap = hub.getSnapshot();
    REQUIRE_FALSE(emptySnap.hasActiveGame);
    REQUIRE_FALSE(emptySnap.upscaler.has_value());
}

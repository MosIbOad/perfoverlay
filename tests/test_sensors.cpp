#include "sensors/sensor_types.hpp"
#include "sensors/sensor_manager.hpp"
#include "sensors/cpu_sensor.hpp"
#include "sensors/cpu_temp_provider.hpp"
#include "sensors/cpu_temp_sensor.hpp"
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
        if (values.cpuTempC.has_value()) snapshot.cpuTempC = values.cpuTempC;
        if (values.gpuUsagePercent.has_value()) snapshot.gpuUsagePercent = values.gpuUsagePercent;
        if (values.gpuTempC.has_value()) snapshot.gpuTempC = values.gpuTempC;
        if (values.gpuVramUsedMB.has_value()) snapshot.gpuVramUsedMB = values.gpuVramUsedMB;
        if (values.gpuVramTotalMB.has_value()) snapshot.gpuVramTotalMB = values.gpuVramTotalMB;
        if (values.gpuClockMHz.has_value()) snapshot.gpuClockMHz = values.gpuClockMHz;
        if (values.gpuPowerW.has_value()) snapshot.gpuPowerW = values.gpuPowerW;
    }
};

class MockHttpClient : public IHttpClient {
public:
    std::optional<std::string> response;
    int callCount{0};

    std::optional<std::string> get(
        std::string_view /*host*/,
        uint16_t /*port*/,
        std::string_view /*path*/,
        std::chrono::milliseconds /*timeout*/
    ) override {
        ++callCount;
        return response;
    }
};

} // namespace

TEST_CASE("SensorSnapshot - Default values are std::nullopt", "[sensors]") {
    SensorSnapshot snapshot;
    REQUIRE_FALSE(snapshot.cpuUsagePercent.has_value());
    REQUIRE_FALSE(snapshot.cpuTempC.has_value());
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

TEST_CASE("NullCpuTempProvider - Always returns std::nullopt", "[sensors][cputemp]") {
    NullCpuTempProvider nullProvider;
    REQUIRE_FALSE(nullProvider.getCpuTempC().has_value());
}

TEST_CASE("parseLhmCpuTempJson - Parses various LHM JSON payloads", "[sensors][cputemp]") {
    SECTION("AMD Ryzen CPU Package and Core (Tdie)") {
        const std::string amdJson = R"json({
            "Text": "Sensor",
            "Children": [
                {
                    "Text": "AMD Ryzen 7 7800X3D",
                    "Children": [
                        {
                            "Text": "Temperatures",
                            "Children": [
                                { "Text": "CPU Package", "Value": "54.2 C" },
                                { "Text": "Core (Tdie)", "Value": "53.8 C" }
                            ]
                        }
                    ]
                }
            ]
        })json";

        const auto temp = parseLhmCpuTempJson(amdJson);
        REQUIRE(temp.has_value());
        REQUIRE(*temp == Catch::Approx(54.2));
    }

    SECTION("AMD Ryzen with Core (Tctl/Tdie) when Package is absent") {
        const std::string amdCoreJson = R"json({
            "Text": "Sensor",
            "Children": [
                {
                    "Text": "AMD Ryzen 5 5600X",
                    "Children": [
                        {
                            "Text": "Temperatures",
                            "Children": [
                                { "Text": "Core (Tctl/Tdie)", "Value": "61.5 C" }
                            ]
                        }
                    ]
                }
            ]
        })json";

        const auto temp = parseLhmCpuTempJson(amdCoreJson);
        REQUIRE(temp.has_value());
        REQUIRE(*temp == Catch::Approx(61.5));
    }

    SECTION("Intel Core CPU with comma decimal formatting") {
        const std::string intelJson = R"json({
            "Text": "Sensor",
            "Children": [
                {
                    "Text": "Intel Core i7-13700K",
                    "Children": [
                        {
                            "Text": "Temperatures",
                            "Children": [
                                { "Text": "CPU Package", "Value": "58,4 C" }
                            ]
                        }
                    ]
                }
            ]
        })json";

        const auto temp = parseLhmCpuTempJson(intelJson);
        REQUIRE(temp.has_value());
        REQUIRE(*temp == Catch::Approx(58.4));
    }

    SECTION("JSON tree with only GPU sensors returns std::nullopt") {
        const std::string gpuOnlyJson = R"json({
            "Text": "Sensor",
            "Children": [
                {
                    "Text": "NVIDIA GeForce RTX 4080",
                    "Children": [
                        {
                            "Text": "Temperatures",
                            "Children": [
                                { "Text": "GPU Core", "Value": "65.0 C" }
                            ]
                        }
                    ]
                }
            ]
        })json";

        const auto temp = parseLhmCpuTempJson(gpuOnlyJson);
        REQUIRE_FALSE(temp.has_value());
    }

    SECTION("Malformed or empty JSON safely returns std::nullopt") {
        REQUIRE_FALSE(parseLhmCpuTempJson("").has_value());
        REQUIRE_FALSE(parseLhmCpuTempJson("Not a json").has_value());
        REQUIRE_FALSE(parseLhmCpuTempJson("{ unclosed json").has_value());
    }
}

TEST_CASE("LibreHardwareMonitorHttpProvider - Queries mock HTTP client", "[sensors][cputemp]") {
    auto mockClient = std::make_shared<MockHttpClient>();

    SECTION("Successful response returns parsed temperature") {
        mockClient->response = R"json({
            "Text": "Sensor",
            "Children": [
                {
                    "Text": "AMD Ryzen 9 7950X",
                    "Children": [
                        {
                            "Text": "Temperatures",
                            "Children": [
                                { "Text": "CPU Package", "Value": "49.0 C" }
                            ]
                        }
                    ]
                }
            ]
        })json";

        LibreHardwareMonitorHttpProvider provider(mockClient, "localhost", 8085, "/data.json");
        const auto temp = provider.getCpuTempC();

        REQUIRE(mockClient->callCount == 1);
        REQUIRE(temp.has_value());
        REQUIRE(*temp == Catch::Approx(49.0));
    }

    SECTION("HTTP connection failure / timeout returns std::nullopt") {
        mockClient->response = std::nullopt; // Server not running

        LibreHardwareMonitorHttpProvider provider(mockClient, "localhost", 8085, "/data.json");
        const auto temp = provider.getCpuTempC();

        REQUIRE(mockClient->callCount == 1);
        REQUIRE_FALSE(temp.has_value());
    }

    SECTION("Server returning 404/garbage returns std::nullopt without crashing") {
        mockClient->response = "<html>404 Not Found</html>";

        LibreHardwareMonitorHttpProvider provider(mockClient, "localhost", 8085, "/data.json");
        const auto temp = provider.getCpuTempC();

        REQUIRE(mockClient->callCount == 1);
        REQUIRE_FALSE(temp.has_value());
    }
}

TEST_CASE("CpuTempSensor - Polling updates SensorSnapshot", "[sensors][cputemp]") {
    auto mockClient = std::make_shared<MockHttpClient>();
    mockClient->response = R"json({
        "Text": "Sensor",
        "Children": [
            {
                "Text": "Intel Core i9-14900K",
                "Children": [
                    {
                        "Text": "Temperatures",
                        "Children": [
                            { "Text": "CPU Package", "Value": "72.5 C" }
                        ]
                    }
                ]
            }
        ]
    })json";

    auto provider = std::make_unique<LibreHardwareMonitorHttpProvider>(mockClient);
    CpuTempSensor sensor(std::move(provider));

    REQUIRE(sensor.init());
    sensor.poll();

    REQUIRE(sensor.getCurrentTempC().has_value());
    REQUIRE(*sensor.getCurrentTempC() == Catch::Approx(72.5));

    SensorSnapshot snap;
    sensor.updateSnapshot(snap);
    REQUIRE(snap.cpuTempC.has_value());
    REQUIRE(*snap.cpuTempC == Catch::Approx(72.5));
}

TEST_CASE("createCpuTempProvider - Factory configuration modes", "[sensors][cputemp]") {
    SECTION("Disabled mode creates NullCpuTempProvider") {
        auto provider = createCpuTempProvider(CpuTempProviderType::Disabled);
        REQUIRE_FALSE(provider->getCpuTempC().has_value());
    }

    SECTION("Auto mode with mock client creates functional LHM provider") {
        auto mockClient = std::make_shared<MockHttpClient>();
        mockClient->response = R"json({
            "Text": "Sensor",
            "Children": [
                {
                    "Text": "CPU",
                    "Children": [
                        { "Text": "CPU Package", "Value": "60.0 C" }
                    ]
                }
            ]
        })json";

        auto provider = createCpuTempProvider(CpuTempProviderType::Auto, mockClient);
        const auto temp = provider->getCpuTempC();
        REQUIRE(temp.has_value());
        REQUIRE(*temp == Catch::Approx(60.0));
    }
}

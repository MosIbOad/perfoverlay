#include "core/config.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace perfoverlay::core;

TEST_CASE("Config - Corner string conversions", "[config]") {
    REQUIRE(cornerToString(OverlayCorner::TopLeft) == "TopLeft");
    REQUIRE(cornerToString(OverlayCorner::TopRight) == "TopRight");
    REQUIRE(cornerToString(OverlayCorner::BottomLeft) == "BottomLeft");
    REQUIRE(cornerToString(OverlayCorner::BottomRight) == "BottomRight");

    REQUIRE(stringToCorner("TopLeft") == OverlayCorner::TopLeft);
    REQUIRE(stringToCorner("topleft") == OverlayCorner::TopLeft);
    REQUIRE(stringToCorner("top_left") == OverlayCorner::TopLeft);
    REQUIRE(stringToCorner("TopRight") == OverlayCorner::TopRight);
    REQUIRE(stringToCorner("top_right") == OverlayCorner::TopRight);
    REQUIRE(stringToCorner("BottomLeft") == OverlayCorner::BottomLeft);
    REQUIRE(stringToCorner("bottom_left") == OverlayCorner::BottomLeft);
    REQUIRE(stringToCorner("BottomRight") == OverlayCorner::BottomRight);
    REQUIRE(stringToCorner("bottom_right") == OverlayCorner::BottomRight);
    REQUIRE(stringToCorner("invalid_corner") == OverlayCorner::TopLeft);
}

TEST_CASE("Config - Full serialization and round-trip parsing", "[config]") {
    AppConfig original;
    original.corner = OverlayCorner::BottomRight;
    original.offsetX = 45;
    original.offsetY = 30;
    original.windowWidth = 380;
    original.windowHeight = 310;
    original.backgroundAlpha = 0.65f;
    original.fontScale = 1.25f;
    original.fpsPollIntervalMs = 200;
    original.sensorPollIntervalMs = 400;
    original.toggleHotkey = "F11";
    original.cpuTempProvider = "librehardwaremonitor";

    original.metrics.showFps = true;
    original.metrics.show1PercentLow = false;
    original.metrics.show01PercentLow = false;
    original.metrics.showFrameTime = true;
    original.metrics.showCpuUsage = false;
    original.metrics.showCpuTemp = true;
    original.metrics.showGpuUsage = true;
    original.metrics.showGpuTemp = true;
    original.metrics.showVram = false;
    original.metrics.showGpuClock = true;
    original.metrics.showGpuPower = false;
    original.metrics.showUpscaler = true;

    const std::string json = ConfigManager::serializeJson(original);
    REQUIRE_FALSE(json.empty());

    AppConfig parsed;
    const bool ok = ConfigManager::parseJson(json, parsed);
    REQUIRE(ok);

    REQUIRE(parsed.corner == OverlayCorner::BottomRight);
    REQUIRE(parsed.offsetX == 45);
    REQUIRE(parsed.offsetY == 30);
    REQUIRE(parsed.windowWidth == 380);
    REQUIRE(parsed.windowHeight == 310);
    REQUIRE(parsed.backgroundAlpha == Catch::Approx(0.65f));
    REQUIRE(parsed.fontScale == Catch::Approx(1.25f));
    REQUIRE(parsed.fpsPollIntervalMs == 200);
    REQUIRE(parsed.sensorPollIntervalMs == 400);
    REQUIRE(parsed.toggleHotkey == "F11");
    REQUIRE(parsed.cpuTempProvider == "librehardwaremonitor");

    REQUIRE(parsed.metrics.showFps == true);
    REQUIRE(parsed.metrics.show1PercentLow == false);
    REQUIRE(parsed.metrics.show01PercentLow == false);
    REQUIRE(parsed.metrics.showFrameTime == true);
    REQUIRE(parsed.metrics.showCpuUsage == false);
    REQUIRE(parsed.metrics.showCpuTemp == true);
    REQUIRE(parsed.metrics.showGpuUsage == true);
    REQUIRE(parsed.metrics.showGpuTemp == true);
    REQUIRE(parsed.metrics.showVram == false);
    REQUIRE(parsed.metrics.showGpuClock == true);
    REQUIRE(parsed.metrics.showGpuPower == false);
    REQUIRE(parsed.metrics.showUpscaler == true);
}

TEST_CASE("Config - Partial JSON merges cleanly with defaults", "[config]") {
    const std::string partialJson = R"json({
        "corner": "TopRight",
        "offsetX": 50,
        "metrics": {
            "showFps": false,
            "showCpuUsage": false
        }
    })json";

    AppConfig parsed;
    const bool ok = ConfigManager::parseJson(partialJson, parsed);
    REQUIRE(ok);

    // Overridden fields
    REQUIRE(parsed.corner == OverlayCorner::TopRight);
    REQUIRE(parsed.offsetX == 50);
    REQUIRE_FALSE(parsed.metrics.showFps);
    REQUIRE_FALSE(parsed.metrics.showCpuUsage);

    // Unspecified fields retain default values
    REQUIRE(parsed.offsetY == 20);
    REQUIRE(parsed.windowWidth == 340);
    REQUIRE(parsed.backgroundAlpha == Catch::Approx(0.78f));
    REQUIRE(parsed.metrics.showGpuUsage == true);
    REQUIRE(parsed.metrics.show1PercentLow == true);
    REQUIRE(parsed.metrics.showUpscaler == true);
    REQUIRE(parsed.toggleHotkey == "F10");
}

TEST_CASE("Config - Bounds clamping on aesthetic parameters", "[config]") {
    const std::string extremeJson = R"json({
        "backgroundAlpha": 5.0,
        "fontScale": -2.0
    })json";

    AppConfig parsed;
    const bool ok = ConfigManager::parseJson(extremeJson, parsed);
    REQUIRE(ok);

    // Alpha clamped to [0.05, 1.0]
    REQUIRE(parsed.backgroundAlpha == Catch::Approx(1.0f));
    // Font scale clamped to [0.5, 3.0]
    REQUIRE(parsed.fontScale == Catch::Approx(0.5f));
}

TEST_CASE("Config - File loading creates default when file is absent", "[config]") {
    const auto tempDir = std::filesystem::temp_directory_path() / "perfoverlay_test_nonexistent";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);

    const auto testFile = tempDir / "config.json";

    const auto result = ConfigManager::loadFromFile(testFile);
    REQUIRE(result.createdDefault);
    REQUIRE_FALSE(result.recoveredFromCorruption);
    REQUIRE(std::filesystem::exists(testFile));

    // Verify written file is valid JSON
    AppConfig reloaded;
    std::ifstream inFile(testFile);
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    REQUIRE(ConfigManager::parseJson(buffer.str(), reloaded));

    std::filesystem::remove_all(tempDir, ec);
}

TEST_CASE("Config - Corrupted JSON file recovery and backup", "[config]") {
    const auto tempDir = std::filesystem::temp_directory_path() / "perfoverlay_test_corrupt";
    std::error_code ec;
    std::filesystem::create_directories(tempDir, ec);

    const auto testFile = tempDir / "config.json";
    const std::string corruptedContent = "{\n  \"corner\": \"TopLeft\",\n  INVALID JSON SYNTAX";

    {
        std::ofstream outFile(testFile);
        outFile << corruptedContent;
    }

    const auto result = ConfigManager::loadFromFile(testFile);
    REQUIRE(result.recoveredFromCorruption);
    REQUIRE_FALSE(result.createdDefault);

    // Verify backup file exists and contains the corrupted content
    const std::filesystem::path backupPath = testFile.string() + ".bak";
    REQUIRE(std::filesystem::exists(backupPath));

    std::ifstream bakFile(backupPath);
    std::stringstream bakBuffer;
    bakBuffer << bakFile.rdbuf();
    REQUIRE(bakBuffer.str() == corruptedContent);

    // Verify active file was replaced with clean defaults
    AppConfig freshConfig;
    std::ifstream freshFile(testFile);
    std::stringstream freshBuffer;
    freshBuffer << freshFile.rdbuf();
    REQUIRE(ConfigManager::parseJson(freshBuffer.str(), freshConfig));

    std::filesystem::remove_all(tempDir, ec);
}

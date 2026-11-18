#define _CRT_SECURE_NO_WARNINGS
#include "core/config.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace perfoverlay::core {

namespace {

std::string toLower(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

} // namespace

std::string cornerToString(OverlayCorner corner) {
    switch (corner) {
    case OverlayCorner::TopRight:
        return "TopRight";
    case OverlayCorner::BottomLeft:
        return "BottomLeft";
    case OverlayCorner::BottomRight:
        return "BottomRight";
    case OverlayCorner::TopLeft:
    default:
        return "TopLeft";
    }
}

OverlayCorner stringToCorner(std::string_view str) {
    const std::string s = toLower(str);
    if (s == "topright" || s == "top_right") {
        return OverlayCorner::TopRight;
    }
    if (s == "bottomleft" || s == "bottom_left") {
        return OverlayCorner::BottomLeft;
    }
    if (s == "bottomright" || s == "bottom_right") {
        return OverlayCorner::BottomRight;
    }
    return OverlayCorner::TopLeft;
}

std::filesystem::path ConfigManager::getDefaultConfigPath() {
    const char* appData = std::getenv("APPDATA");
    if (appData && *appData) {
        return std::filesystem::path(appData) / "perfoverlay" / "config.json";
    }
    return std::filesystem::current_path() / "config.json";
}

bool ConfigManager::parseJson(std::string_view jsonStr, AppConfig& outConfig) {
    if (jsonStr.empty()) {
        return false;
    }

    try {
        const auto j = nlohmann::json::parse(jsonStr);
        if (!j.is_object()) {
            return false;
        }

        // Overlay placement
        if (j.contains("corner") && j["corner"].is_string()) {
            outConfig.corner = stringToCorner(j["corner"].get<std::string>());
        }
        if (j.contains("offsetX") && j["offsetX"].is_number()) {
            outConfig.offsetX = j["offsetX"].get<int>();
        }
        if (j.contains("offsetY") && j["offsetY"].is_number()) {
            outConfig.offsetY = j["offsetY"].get<int>();
        }
        if (j.contains("windowWidth") && j["windowWidth"].is_number()) {
            outConfig.windowWidth = j["windowWidth"].get<int>();
        }
        if (j.contains("windowHeight") && j["windowHeight"].is_number()) {
            outConfig.windowHeight = j["windowHeight"].get<int>();
        }

        // Aesthetics
        if (j.contains("backgroundAlpha") && j["backgroundAlpha"].is_number()) {
            outConfig.backgroundAlpha = std::clamp(j["backgroundAlpha"].get<float>(), 0.05f, 1.0f);
        }
        if (j.contains("fontScale") && j["fontScale"].is_number()) {
            outConfig.fontScale = std::clamp(j["fontScale"].get<float>(), 0.5f, 3.0f);
        }

        // Polling intervals
        if (j.contains("fpsPollIntervalMs") && j["fpsPollIntervalMs"].is_number_unsigned()) {
            outConfig.fpsPollIntervalMs = j["fpsPollIntervalMs"].get<uint32_t>();
        }
        if (j.contains("sensorPollIntervalMs") && j["sensorPollIntervalMs"].is_number_unsigned()) {
            outConfig.sensorPollIntervalMs = j["sensorPollIntervalMs"].get<uint32_t>();
        }

        // Hotkey & Providers
        if (j.contains("toggleHotkey") && j["toggleHotkey"].is_string()) {
            outConfig.toggleHotkey = j["toggleHotkey"].get<std::string>();
        }
        if (j.contains("cpuTempProvider") && j["cpuTempProvider"].is_string()) {
            outConfig.cpuTempProvider = toLower(j["cpuTempProvider"].get<std::string>());
        }

        // Metric visibility flags (merging with defaults)
        if (j.contains("metrics") && j["metrics"].is_object()) {
            const auto& m = j["metrics"];
            if (m.contains("showFps") && m["showFps"].is_boolean()) {
                outConfig.metrics.showFps = m["showFps"].get<bool>();
            }
            if (m.contains("show1PercentLow") && m["show1PercentLow"].is_boolean()) {
                outConfig.metrics.show1PercentLow = m["show1PercentLow"].get<bool>();
            }
            if (m.contains("show01PercentLow") && m["show01PercentLow"].is_boolean()) {
                outConfig.metrics.show01PercentLow = m["show01PercentLow"].get<bool>();
            }
            if (m.contains("showFrameTime") && m["showFrameTime"].is_boolean()) {
                outConfig.metrics.showFrameTime = m["showFrameTime"].get<bool>();
            }
            if (m.contains("showCpuUsage") && m["showCpuUsage"].is_boolean()) {
                outConfig.metrics.showCpuUsage = m["showCpuUsage"].get<bool>();
            }
            if (m.contains("showCpuTemp") && m["showCpuTemp"].is_boolean()) {
                outConfig.metrics.showCpuTemp = m["showCpuTemp"].get<bool>();
            }
            if (m.contains("showGpuUsage") && m["showGpuUsage"].is_boolean()) {
                outConfig.metrics.showGpuUsage = m["showGpuUsage"].get<bool>();
            }
            if (m.contains("showGpuTemp") && m["showGpuTemp"].is_boolean()) {
                outConfig.metrics.showGpuTemp = m["showGpuTemp"].get<bool>();
            }
            if (m.contains("showVram") && m["showVram"].is_boolean()) {
                outConfig.metrics.showVram = m["showVram"].get<bool>();
            }
            if (m.contains("showGpuClock") && m["showGpuClock"].is_boolean()) {
                outConfig.metrics.showGpuClock = m["showGpuClock"].get<bool>();
            }
            if (m.contains("showGpuPower") && m["showGpuPower"].is_boolean()) {
                outConfig.metrics.showGpuPower = m["showGpuPower"].get<bool>();
            }
            if (m.contains("showUpscaler") && m["showUpscaler"].is_boolean()) {
                outConfig.metrics.showUpscaler = m["showUpscaler"].get<bool>();
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

std::string ConfigManager::serializeJson(const AppConfig& c, int indent) {
    nlohmann::json j;
    j["corner"] = cornerToString(c.corner);
    j["offsetX"] = c.offsetX;
    j["offsetY"] = c.offsetY;
    j["windowWidth"] = c.windowWidth;
    j["windowHeight"] = c.windowHeight;
    j["backgroundAlpha"] = c.backgroundAlpha;
    j["fontScale"] = c.fontScale;
    j["fpsPollIntervalMs"] = c.fpsPollIntervalMs;
    j["sensorPollIntervalMs"] = c.sensorPollIntervalMs;
    j["toggleHotkey"] = c.toggleHotkey;
    j["cpuTempProvider"] = c.cpuTempProvider;

    j["metrics"] = {
        {"showFps", c.metrics.showFps},
        {"show1PercentLow", c.metrics.show1PercentLow},
        {"show01PercentLow", c.metrics.show01PercentLow},
        {"showFrameTime", c.metrics.showFrameTime},
        {"showCpuUsage", c.metrics.showCpuUsage},
        {"showCpuTemp", c.metrics.showCpuTemp},
        {"showGpuUsage", c.metrics.showGpuUsage},
        {"showGpuTemp", c.metrics.showGpuTemp},
        {"showVram", c.metrics.showVram},
        {"showGpuClock", c.metrics.showGpuClock},
        {"showGpuPower", c.metrics.showGpuPower},
        {"showUpscaler", c.metrics.showUpscaler}
    };

    return j.dump(indent);
}

ConfigLoadResult ConfigManager::loadFromFile(const std::filesystem::path& filePath) {
    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec)) {
        // Create directory hierarchy and save default configuration
        if (!filePath.parent_path().empty()) {
            std::filesystem::create_directories(filePath.parent_path(), ec);
        }
        AppConfig defaultConfig;
        saveToFile(filePath, defaultConfig);
        return ConfigLoadResult{
            .config = defaultConfig,
            .createdDefault = true,
            .recoveredFromCorruption = false,
            .backupFilePath = ""
        };
    }

    std::ifstream inFile(filePath, std::ios::in | std::ios::binary);
    if (!inFile.is_open()) {
        AppConfig fallback;
        return ConfigLoadResult{.config = fallback};
    }

    std::stringstream buffer;
    buffer << inFile.rdbuf();
    inFile.close();

    const std::string content = buffer.str();
    AppConfig loadedConfig;

    if (parseJson(content, loadedConfig)) {
        return ConfigLoadResult{
            .config = loadedConfig,
            .createdDefault = false,
            .recoveredFromCorruption = false,
            .backupFilePath = ""
        };
    }

    // Corrupted file detected: create a backup copy and reset to defaults
    const std::string backupPath = filePath.string() + ".bak";
    std::filesystem::copy_file(
        filePath,
        backupPath,
        std::filesystem::copy_options::overwrite_existing,
        ec
    );

    AppConfig freshDefault;
    saveToFile(filePath, freshDefault);

    return ConfigLoadResult{
        .config = freshDefault,
        .createdDefault = false,
        .recoveredFromCorruption = true,
        .backupFilePath = backupPath
    };
}

bool ConfigManager::saveToFile(const std::filesystem::path& filePath, const AppConfig& config) {
    std::error_code ec;
    if (!filePath.parent_path().empty()) {
        std::filesystem::create_directories(filePath.parent_path(), ec);
    }

    std::ofstream outFile(filePath, std::ios::out | std::ios::trunc);
    if (!outFile.is_open()) {
        return false;
    }

    const std::string serialized = serializeJson(config);
    outFile.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
    return outFile.good();
}

} // namespace perfoverlay::core

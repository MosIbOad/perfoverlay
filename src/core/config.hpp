#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace perfoverlay::core {

/**
 * @brief Screen corner options for placing the overlay.
 */
enum class OverlayCorner { TopLeft, TopRight, BottomLeft, BottomRight };

[[nodiscard]] std::string cornerToString(OverlayCorner corner);
[[nodiscard]] OverlayCorner stringToCorner(std::string_view str);

/**
 * @brief Fine-grained metric visibility toggles.
 */
struct VisibleMetricsConfig {
    bool showFps{true};
    bool show1PercentLow{true};
    bool show01PercentLow{true};
    bool showFrameTime{true};
    bool showCpuUsage{true};
    bool showCpuTemp{true};
    bool showGpuUsage{true};
    bool showGpuTemp{true};
    bool showVram{true};
    bool showGpuClock{true};
    bool showGpuPower{true};
    bool showUpscaler{true};
};

/**
 * @brief Complete application configuration.
 */
struct AppConfig {
    OverlayCorner corner{OverlayCorner::TopLeft};
    int offsetX{20};
    int offsetY{20};
    int windowWidth{340};
    int windowHeight{290};

    float backgroundAlpha{0.78f};
    float fontScale{1.0f};

    uint32_t fpsPollIntervalMs{250};
    uint32_t sensorPollIntervalMs{500};

    std::string toggleHotkey{"F10"};
    std::string cpuTempProvider{"auto"};  // "auto", "librehardwaremonitor", "disabled"

    VisibleMetricsConfig metrics;
};

/**
 * @brief Result status of loading configuration.
 */
struct ConfigLoadResult {
    AppConfig config;
    bool createdDefault{false};
    bool recoveredFromCorruption{false};
    std::string backupFilePath;
};

/**
 * @brief Configuration serialization, merging, and file persistence.
 */
class ConfigManager {
public:
    ConfigManager() = default;

    /**
     * @brief Resolves default %APPDATA%\perfoverlay\config.json path.
     */
    [[nodiscard]] static std::filesystem::path getDefaultConfigPath();

    /**
     * @brief Loads configuration from file. If missing, creates defaults.
     *        If corrupt, backs up corrupt file and falls back to defaults safely.
     */
    [[nodiscard]] static ConfigLoadResult loadFromFile(const std::filesystem::path& filePath);

    /**
     * @brief Saves configuration to the specified file in formatted JSON.
     */
    static bool saveToFile(const std::filesystem::path& filePath, const AppConfig& config);

    /**
     * @brief Pure JSON parser with partial field merging against defaults.
     *        Does not throw. Returns std::nullopt on parse error.
     */
    [[nodiscard]] static bool parseJson(std::string_view jsonStr, AppConfig& outConfig);

    /**
     * @brief Serializes AppConfig to formatted JSON string.
     */
    [[nodiscard]] static std::string serializeJson(const AppConfig& config, int indent = 4);
};

}  // namespace perfoverlay::core

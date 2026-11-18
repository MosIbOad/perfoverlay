#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace perfoverlay::sensors {

/**
 * @brief Common interface for CPU temperature telemetry providers.
 */
class ICpuTempProvider {
public:
    virtual ~ICpuTempProvider() = default;

    /**
     * @brief Queries the latest CPU package or core temperature in Celsius.
     * @return Temperature in °C, or std::nullopt if unavailable.
     */
    [[nodiscard]] virtual std::optional<double> getCpuTempC() = 0;
};

/**
 * @brief Safe fallback provider that always returns std::nullopt ("N/A").
 *        Requires zero drivers, zero background services, zero permissions.
 */
class NullCpuTempProvider : public ICpuTempProvider {
public:
    ~NullCpuTempProvider() override = default;

    [[nodiscard]] std::optional<double> getCpuTempC() override {
        return std::nullopt;
    }
};

/**
 * @brief Abstract HTTP client interface enabling unit testing without live network connections.
 */
class IHttpClient {
public:
    virtual ~IHttpClient() = default;

    /**
     * @brief Performs a synchronous HTTP GET request with a strict timeout.
     * @return Response body as string on success, std::nullopt on timeout or error.
     */
    [[nodiscard]] virtual std::optional<std::string> get(
        std::string_view host,
        uint16_t port,
        std::string_view path,
        std::chrono::milliseconds timeout
    ) = 0;
};

/**
 * @brief Windows WinHTTP-based client implementation (uses winhttp.dll).
 *        No third-party networking dependencies.
 */
class WinHttpClient : public IHttpClient {
public:
    WinHttpClient() = default;
    ~WinHttpClient() override = default;

    [[nodiscard]] std::optional<std::string> get(
        std::string_view host,
        uint16_t port,
        std::string_view path,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(300)
    ) override;
};

/**
 * @brief CPU temperature provider querying LibreHardwareMonitor's Remote Web Server.
 *
 * SAFETY & ANTI-CHEAT ARCHITECTURE:
 * - Requires NO kernel drivers installed or loaded by perfoverlay.
 * - LibreHardwareMonitor is launched independently by the user as an external tool.
 * - Queries http://localhost:8085/data.json with a short timeout (300 ms).
 * - If LibreHardwareMonitor is not running or unreachable, fails silently and returns std::nullopt.
 */
class LibreHardwareMonitorHttpProvider : public ICpuTempProvider {
public:
    explicit LibreHardwareMonitorHttpProvider(
        std::shared_ptr<IHttpClient> httpClient = std::make_shared<WinHttpClient>(),
        std::string host = "localhost",
        uint16_t port = 8085,
        std::string path = "/data.json"
    );
    ~LibreHardwareMonitorHttpProvider() override = default;

    [[nodiscard]] std::optional<double> getCpuTempC() override;

private:
    std::shared_ptr<IHttpClient> httpClient_;
    std::string host_;
    uint16_t port_{8085};
    std::string path_{"/data.json"};
};

/**
 * @brief Configuration modes for selecting CPU temperature telemetry provider.
 */
enum class CpuTempProviderType {
    Disabled,              ///< Always N/A (NullCpuTempProvider)
    LibreHardwareMonitor,  ///< Specifically use LibreHardwareMonitor Remote Web Server
    Auto                   ///< Automatically detect: query LHM, return N/A if unavailable
};

/**
 * @brief Pure helper parsing LibreHardwareMonitor's JSON sensor tree to extract CPU temperature.
 *        Exposed for unit testing.
 * @param jsonString Raw JSON body returned by /data.json.
 * @return CPU temperature in °C, or std::nullopt if not found or malformed.
 */
[[nodiscard]] std::optional<double> parseLhmCpuTempJson(const std::string& jsonString);

/**
 * @brief Factory function creating the desired CPU temperature provider based on configuration.
 */
[[nodiscard]] std::unique_ptr<ICpuTempProvider> createCpuTempProvider(
    CpuTempProviderType type = CpuTempProviderType::Auto,
    std::shared_ptr<IHttpClient> httpClient = nullptr
);

} // namespace perfoverlay::sensors

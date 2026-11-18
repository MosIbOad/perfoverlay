#include "sensors/cpu_temp_provider.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <vector>

namespace perfoverlay::sensors {

namespace {

std::string toLower(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

std::optional<double> parseTemperatureString(std::string_view valStr) {
    if (valStr.empty()) {
        return std::nullopt;
    }

    size_t i = 0;
    while (i < valStr.size() && std::isspace(static_cast<unsigned char>(valStr[i]))) {
        ++i;
    }

    std::string numStr;
    bool foundDecimal = false;
    for (; i < valStr.size(); ++i) {
        const char c = valStr[i];
        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '-' && numStr.empty())) {
            numStr.push_back(c);
        } else if ((c == '.' || c == ',') && !foundDecimal) {
            numStr.push_back('.');
            foundDecimal = true;
        } else if (c == ' ' || c == '\t' || static_cast<unsigned char>(c) == 0xC2 ||
                   static_cast<unsigned char>(c) == 0xB0 || c == 'C' || c == 'c') {
            break;
        } else {
            break;
        }
    }

    if (numStr.empty() || numStr == "-") {
        return std::nullopt;
    }

    try {
        return std::stod(numStr);
    } catch (...) {
        return std::nullopt;
    }
}

struct TempCandidate {
    int priority{0};  // 1 = highest (CPU Package), 5 = lowest
    double value{0.0};
};

void findCpuTempInNode(const nlohmann::json& node, bool isUnderCpu, std::vector<TempCandidate>& candidates) {
    if (!node.is_object()) {
        return;
    }

    std::string text;
    if (node.contains("Text") && node["Text"].is_string()) {
        text = node["Text"].get<std::string>();
    } else if (node.contains("text") && node["text"].is_string()) {
        text = node["text"].get<std::string>();
    }

    std::string sensorId;
    if (node.contains("SensorId") && node["SensorId"].is_string()) {
        sensorId = node["SensorId"].get<std::string>();
    }

    const std::string lowerText = toLower(text);
    const std::string lowerId = toLower(sensorId);

    const bool currentIsCpu =
        isUnderCpu || lowerId.find("cpu") != std::string::npos || lowerText.find("cpu") != std::string::npos ||
        lowerText.find("ryzen") != std::string::npos || lowerText.find("intel") != std::string::npos;

    // Check value if present
    std::string valStr;
    if (node.contains("Value") && node["Value"].is_string()) {
        valStr = node["Value"].get<std::string>();
    } else if (node.contains("value") && node["value"].is_string()) {
        valStr = node["value"].get<std::string>();
    }

    if (!valStr.empty()) {
        const auto parsedVal = parseTemperatureString(valStr);
        if (parsedVal.has_value()) {
            if (lowerText == "cpu package" || lowerId.find("cpu_package") != std::string::npos) {
                candidates.push_back({1, *parsedVal});
            } else if (lowerText == "core (tdie)" || lowerText == "core (tctl/tdie)" || lowerText == "tctl/tdie") {
                candidates.push_back({2, *parsedVal});
            } else if (currentIsCpu && (lowerText == "package" || lowerText == "cpu total")) {
                candidates.push_back({3, *parsedVal});
            } else if (currentIsCpu && (lowerText == "core max" || lowerText == "cpu core")) {
                candidates.push_back({4, *parsedVal});
            } else if (currentIsCpu && lowerText.find("temp") != std::string::npos &&
                       lowerText.find("distance") == std::string::npos) {
                candidates.push_back({5, *parsedVal});
            }
        }
    }

    // Traverse children
    const char* childrenKey =
        node.contains("Children") ? "Children" : (node.contains("children") ? "children" : nullptr);

    if (childrenKey && node[childrenKey].is_array()) {
        for (const auto& child : node[childrenKey]) {
            findCpuTempInNode(child, currentIsCpu, candidates);
        }
    }
}

}  // namespace

std::optional<std::string> WinHttpClient::get(std::string_view host, uint16_t port, std::string_view path,
                                              std::chrono::milliseconds timeout) {
    HINTERNET hSession = WinHttpOpen(L"perfoverlay/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        return std::nullopt;
    }

    const int timeoutMs = static_cast<int>(timeout.count());
    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    const std::wstring wHost(host.begin(), host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }

    const std::wstring wPath(path.begin(), path.end());
    HINTERNET hRequest =
        WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                           0  // Standard HTTP
        );
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return std::nullopt;
    }

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, nullptr);
    }

    std::string responseBody;
    if (bResults) {
        DWORD dwSize = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) {
                break;
            }

            std::vector<char> buffer(dwSize);
            DWORD dwDownloaded = 0;
            if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded) && dwDownloaded > 0) {
                responseBody.append(buffer.data(), dwDownloaded);
            } else {
                break;
            }
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (!bResults || responseBody.empty()) {
        return std::nullopt;
    }

    return responseBody;
}

std::optional<double> parseLhmCpuTempJson(const std::string& jsonString) {
    if (jsonString.empty()) {
        return std::nullopt;
    }

    try {
        const auto root = nlohmann::json::parse(jsonString);
        std::vector<TempCandidate> candidates;
        findCpuTempInNode(root, false, candidates);

        if (candidates.empty()) {
            return std::nullopt;
        }

        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& a, const auto& b) { return a.priority < b.priority; });

        return candidates.front().value;
    } catch (...) {
        return std::nullopt;
    }
}

LibreHardwareMonitorHttpProvider::LibreHardwareMonitorHttpProvider(std::shared_ptr<IHttpClient> httpClient,
                                                                   std::string host, uint16_t port, std::string path)
    : httpClient_(std::move(httpClient)), host_(std::move(host)), port_(port), path_(std::move(path)) {}

std::optional<double> LibreHardwareMonitorHttpProvider::getCpuTempC() {
    if (!httpClient_) {
        return std::nullopt;
    }

    const auto response = httpClient_->get(host_, port_, path_, std::chrono::milliseconds(300));
    if (!response.has_value() || response->empty()) {
        return std::nullopt;
    }

    return parseLhmCpuTempJson(*response);
}

std::unique_ptr<ICpuTempProvider> createCpuTempProvider(CpuTempProviderType type,
                                                        std::shared_ptr<IHttpClient> httpClient) {
    switch (type) {
        case CpuTempProviderType::Disabled:
            return std::make_unique<NullCpuTempProvider>();

        case CpuTempProviderType::LibreHardwareMonitor:
        case CpuTempProviderType::Auto:
        default:
            if (httpClient) {
                return std::make_unique<LibreHardwareMonitorHttpProvider>(std::move(httpClient));
            }
            return std::make_unique<LibreHardwareMonitorHttpProvider>();
    }
}

}  // namespace perfoverlay::sensors

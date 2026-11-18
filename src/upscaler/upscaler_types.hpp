#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace perfoverlay::upscaler {

/**
 * @brief Categorization of modern real-time upscaling and frame generation technologies.
 */
enum class UpscalerType {
    Dlss,          ///< NVIDIA DLSS Super Resolution
    DlssFrameGen,  ///< NVIDIA DLSS 3 Frame Generation
    Fsr,           ///< AMD FidelityFX Super Resolution
    Xess           ///< Intel XeSS (Xe Super Sampling)
};

/**
 * @brief Extensible definition for identifying known upscaler runtime libraries.
 */
struct KnownDllRule {
    std::string pattern;     ///< Filename or wildcard glob (e.g. "nvngx_dlss.dll", "amd_fidelityfx_*.dll")
    UpscalerType type;       ///< Upscaler technology category
    std::string versionTag;  ///< Optional version identifier (e.g. "2", "3", "SDK", "DX12")
};

/**
 * @brief Performs case-insensitive glob matching (* matches 0 or more chars, ? matches 1 char).
 *        Iterative O(N) implementation with zero heap allocations.
 */
[[nodiscard]] inline bool matchGlob(std::string_view text, std::string_view pattern) noexcept {
    size_t t = 0;
    size_t p = 0;
    size_t starIdx = std::string_view::npos;
    size_t match = 0;

    auto toLower = [](char c) noexcept { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };

    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || toLower(text[t]) == toLower(pattern[p]))) {
            ++t;
            ++p;
        } else if (p < pattern.size() && pattern[p] == '*') {
            starIdx = p;
            match = t;
            ++p;
        } else if (starIdx != std::string_view::npos) {
            p = starIdx + 1;
            ++match;
            t = match;
        } else {
            return false;
        }
    }

    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }

    return p == pattern.size();
}

/**
 * @brief Summary of upscalers detected in the game files/directory.
 */
struct UpscalerInfo {
    bool dlss{false};
    bool dlssFrameGen{false};
    std::optional<std::string> fsr;  ///< e.g. "FSR 3", "FSR 2", or "FSR"
    bool xess{false};

    [[nodiscard]] bool anyDetected() const noexcept { return dlss || dlssFrameGen || fsr.has_value() || xess; }

    /**
     * @brief Formats detected upscalers into a concise, readable comma-separated string.
     *        Example: "DLSS, DLSS-FG, FSR 3, XeSS"
     */
    [[nodiscard]] std::string formatSummary() const {
        std::vector<std::string> items;
        if (dlss) {
            items.emplace_back("DLSS");
        }
        if (dlssFrameGen) {
            items.emplace_back("DLSS-FG");
        }
        if (fsr.has_value()) {
            items.push_back(*fsr);
        }
        if (xess) {
            items.emplace_back("XeSS");
        }

        if (items.empty()) {
            return "None";
        }

        std::string summary;
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) {
                summary += ", ";
            }
            summary += items[i];
        }
        return summary;
    }
};

/**
 * @brief Returns the default extensible rule set for detecting DLSS, FSR, and XeSS libraries.
 */
inline std::vector<KnownDllRule> getDefaultKnownDllRules() {
    return {// NVIDIA DLSS & Frame Generation
            {"nvngx_dlss.dll", UpscalerType::Dlss, ""},
            {"nvngx_dlssg.dll", UpscalerType::DlssFrameGen, ""},
            {"nvngx_dlssd.dll", UpscalerType::Dlss, "RayReconstruction"},

            // AMD FSR (FidelityFX Super Resolution)
            {"ffx_fsr3_api_*.dll", UpscalerType::Fsr, "3"},
            {"ffx_fsr2_api_*.dll", UpscalerType::Fsr, "2"},
            {"ffx_fsr*.dll", UpscalerType::Fsr, ""},
            {"amd_fidelityfx_dx12.dll", UpscalerType::Fsr, "SDK"},
            {"amd_fidelityfx_vk.dll", UpscalerType::Fsr, "SDK"},
            {"amd_fidelityfx_*.dll", UpscalerType::Fsr, "SDK"},
            {"fidelityfx.dll", UpscalerType::Fsr, ""},
            {"fidelityfx_*.dll", UpscalerType::Fsr, ""},

            // Intel XeSS
            {"libxess.dll", UpscalerType::Xess, ""},
            {"libxess_dx11.dll", UpscalerType::Xess, "DX11"},
            {"libxess_*.dll", UpscalerType::Xess, ""}};
}

}  // namespace perfoverlay::upscaler

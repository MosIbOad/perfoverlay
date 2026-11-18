#pragma once

#include "core/metrics_hub.hpp"
#include "upscaler/filesystem.hpp"
#include "upscaler/upscaler_types.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace perfoverlay::upscaler {

/**
 * @brief Detects upscaler technology (DLSS, DLSS Frame Generation, FSR, XeSS)
 *        by inspecting game executable directories.
 *
 * ANTI-CHEAT SAFETY:
 * - Does not inject code or hook game processes.
 * - Does not inspect game process memory or call EnumProcessModules.
 * - Queries only process image path via PROCESS_QUERY_LIMITED_INFORMATION.
 * - Scans filesystem directory containing the executable up to a bounded depth limit.
 * - Caches scan results per process ID and executable path to eliminate ongoing disk I/O.
 */
class UpscalerDetector : public core::IUpscalerProvider {
public:
    explicit UpscalerDetector(std::shared_ptr<IFileSystem> fs = std::make_shared<RealFileSystem>(),
                              size_t maxDepth = 2);
    ~UpscalerDetector() override = default;

    UpscalerDetector(const UpscalerDetector&) = delete;
    UpscalerDetector& operator=(const UpscalerDetector&) = delete;
    UpscalerDetector(UpscalerDetector&&) = delete;
    UpscalerDetector& operator=(UpscalerDetector&&) = delete;

    /**
     * @brief Queries upscaler capabilities for the active process.
     *        Returns cached results immediately; scans directory on first encounter.
     *
     * @param processId Process ID of the active game.
     * @param executablePath Optional full path to the executable. If empty, resolved safely via PID.
     * @return UpscalerInfo indicating detected technologies in game files.
     */
    std::optional<UpscalerInfo> getUpscalerInfo(uint32_t processId, std::string_view executablePath) override;

    /**
     * @brief Performs a direct, non-cached directory scan up to maxDepth.
     *        Useful for diagnostics, testing, or manual directory scans.
     */
    [[nodiscard]] UpscalerInfo scanDirectory(const std::filesystem::path& dirPath) const;

    /**
     * @brief Clears the internal detection cache.
     */
    void clearCache();

    /**
     * @brief Adds a custom rule to the active detection table.
     */
    void addRule(KnownDllRule rule);

    /**
     * @brief Replaces all rules in the active detection table.
     */
    void setRules(std::vector<KnownDllRule> rules);

    /**
     * @brief Retrieves the active detection rules.
     */
    [[nodiscard]] const std::vector<KnownDllRule>& getRules() const noexcept;

    /**
     * @brief Sets the maximum directory recursion depth (default 2).
     */
    void setMaxDepth(size_t depth) noexcept { maxDepth_ = depth; }
    [[nodiscard]] size_t getMaxDepth() const noexcept { return maxDepth_; }

private:
    void scanDirectoryRecursive(const std::filesystem::path& currentDir, size_t currentDepth, UpscalerInfo& info,
                                std::unordered_set<std::string>& visitedDirs) const;

    static void applyRuleMatch(UpscalerInfo& info, const KnownDllRule& rule);

    std::shared_ptr<IFileSystem> fs_;
    size_t maxDepth_{2};
    std::vector<KnownDllRule> rules_;

    struct CacheEntry {
        std::string executablePath;
        UpscalerInfo info;
    };

    mutable std::mutex cacheMutex_;
    std::unordered_map<uint32_t, CacheEntry> cacheByPid_;
    std::unordered_map<std::string, UpscalerInfo> cacheByPath_;
};

}  // namespace perfoverlay::upscaler

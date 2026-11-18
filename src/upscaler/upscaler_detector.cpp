#include "upscaler/upscaler_detector.hpp"
#include "upscaler/process_path.hpp"

#include <algorithm>
#include <cctype>

namespace perfoverlay::upscaler {

namespace {

std::string normalizePath(std::string_view pathStr) {
    std::string result(pathStr);
    std::replace(result.begin(), result.end(), '\\', '/');
    while (result.size() > 1 && result.back() == '/') {
        result.pop_back();
    }
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return result;
}

} // namespace

UpscalerDetector::UpscalerDetector(std::shared_ptr<IFileSystem> fs, size_t maxDepth)
    : fs_(std::move(fs)),
      maxDepth_(maxDepth),
      rules_(getDefaultKnownDllRules()) {}

void UpscalerDetector::applyRuleMatch(UpscalerInfo& info, const KnownDllRule& rule) {
    switch (rule.type) {
    case UpscalerType::Dlss:
        info.dlss = true;
        break;
    case UpscalerType::DlssFrameGen:
        info.dlssFrameGen = true;
        break;
    case UpscalerType::Xess:
        info.xess = true;
        break;
    case UpscalerType::Fsr:
        if (rule.versionTag == "3") {
            info.fsr = "FSR 3";
        } else if (rule.versionTag == "2") {
            if (!info.fsr.has_value() || *info.fsr == "FSR") {
                info.fsr = "FSR 2";
            }
        } else if (!info.fsr.has_value()) {
            info.fsr = "FSR";
        }
        break;
    }
}

void UpscalerDetector::scanDirectoryRecursive(
    const std::filesystem::path& currentDir,
    size_t currentDepth,
    UpscalerInfo& info,
    std::unordered_set<std::string>& visitedDirs
) const {
    const std::string normDir = normalizePath(currentDir.generic_string());
    if (visitedDirs.contains(normDir)) {
        return;
    }
    if (visitedDirs.size() >= 100) {
        return; // Guard against runaway traversal or excessive tree depth
    }
    visitedDirs.insert(normDir);

    const auto entries = fs_->listDirectory(currentDir);
    for (const auto& entry : entries) {
        if (!entry.isDirectory) {
            const std::string filename = entry.path.filename().string();
            for (const auto& rule : rules_) {
                if (matchGlob(filename, rule.pattern)) {
                    applyRuleMatch(info, rule);
                }
            }
        } else if (currentDepth < maxDepth_) {
            scanDirectoryRecursive(entry.path, currentDepth + 1, info, visitedDirs);
        }
    }
}

UpscalerInfo UpscalerDetector::scanDirectory(const std::filesystem::path& dirPath) const {
    UpscalerInfo info;
    if (!fs_->exists(dirPath) || !fs_->isDirectory(dirPath)) {
        return info;
    }

    std::unordered_set<std::string> visitedDirs;
    scanDirectoryRecursive(dirPath, 0, info, visitedDirs);
    return info;
}

std::optional<UpscalerInfo> UpscalerDetector::getUpscalerInfo(
    uint32_t processId,
    std::string_view executablePath
) {
    if (processId == 0) {
        return std::nullopt;
    }

    std::string pathStr(executablePath);
    if (pathStr.empty()) {
        pathStr = getProcessExecutablePath(processId);
    }

    const std::string normPath = normalizePath(pathStr);

    {
        std::lock_guard lock(cacheMutex_);
        auto itPid = cacheByPid_.find(processId);
        if (itPid != cacheByPid_.end()) {
            if (normPath.empty() || itPid->second.executablePath == normPath) {
                return itPid->second.info;
            }
        }

        if (!normPath.empty()) {
            auto itPath = cacheByPath_.find(normPath);
            if (itPath != cacheByPath_.end()) {
                cacheByPid_[processId] = CacheEntry{normPath, itPath->second};
                return itPath->second;
            }
        }
    }

    if (normPath.empty()) {
        return std::nullopt;
    }

    const std::filesystem::path exePath(pathStr);
    const std::filesystem::path dirPath = exePath.parent_path();
    if (dirPath.empty()) {
        return std::nullopt;
    }

    const UpscalerInfo scanned = scanDirectory(dirPath);

    {
        std::lock_guard lock(cacheMutex_);
        cacheByPath_[normPath] = scanned;
        cacheByPid_[processId] = CacheEntry{normPath, scanned};
    }

    return scanned;
}

void UpscalerDetector::clearCache() {
    std::lock_guard lock(cacheMutex_);
    cacheByPid_.clear();
    cacheByPath_.clear();
}

void UpscalerDetector::addRule(KnownDllRule rule) {
    rules_.push_back(std::move(rule));
}

void UpscalerDetector::setRules(std::vector<KnownDllRule> rules) {
    rules_ = std::move(rules);
}

const std::vector<KnownDllRule>& UpscalerDetector::getRules() const noexcept {
    return rules_;
}

} // namespace perfoverlay::upscaler

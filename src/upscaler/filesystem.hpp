#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace perfoverlay::upscaler {

/**
 * @brief Represents an individual file or directory entry returned by IFileSystem.
 */
struct FileEntry {
    std::filesystem::path path;
    bool isDirectory{false};
};

/**
 * @brief Abstract filesystem interface enabling zero-disk-access mocking in unit tests.
 */
class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    [[nodiscard]] virtual bool exists(const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual bool isDirectory(const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual std::vector<FileEntry> listDirectory(const std::filesystem::path& path) const = 0;
};

/**
 * @brief Real filesystem implementation backed by std::filesystem.
 *        Never throws exceptions; silently handles access denied or IO errors via std::error_code.
 */
class RealFileSystem : public IFileSystem {
public:
    RealFileSystem() = default;
    ~RealFileSystem() override = default;

    [[nodiscard]] bool exists(const std::filesystem::path& path) const override;
    [[nodiscard]] bool isDirectory(const std::filesystem::path& path) const override;
    [[nodiscard]] std::vector<FileEntry> listDirectory(const std::filesystem::path& path) const override;
};

} // namespace perfoverlay::upscaler

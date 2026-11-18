#include "upscaler/filesystem.hpp"

#include <system_error>

namespace perfoverlay::upscaler {

bool RealFileSystem::exists(const std::filesystem::path& path) const {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool RealFileSystem::isDirectory(const std::filesystem::path& path) const {
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

std::vector<FileEntry> RealFileSystem::listDirectory(const std::filesystem::path& path) const {
    std::vector<FileEntry> entries;
    std::error_code ec;

    std::filesystem::directory_iterator it(
        path,
        std::filesystem::directory_options::skip_permission_denied,
        ec
    );

    if (ec) {
        return entries;
    }

    for (const auto& entry : it) {
        std::error_code statusEc;
        const bool isDir = entry.is_directory(statusEc);
        entries.push_back(FileEntry{
            .path = entry.path(),
            .isDirectory = !statusEc && isDir,
        });
    }

    return entries;
}

} // namespace perfoverlay::upscaler

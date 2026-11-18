#include "upscaler/filesystem.hpp"
#include "upscaler/upscaler_detector.hpp"
#include "upscaler/upscaler_types.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <vector>

using namespace perfoverlay::upscaler;

namespace {

/**
 * @brief Thread-safe in-memory virtual filesystem for unit tests.
 *        Requires zero disk access and tests arbitrary directory hierarchies.
 */
class MockFileSystem : public IFileSystem {
public:
    void addFile(const std::filesystem::path& filePath) {
        files_.insert(normalize(filePath));
        auto parent = filePath.parent_path();
        while (!parent.empty() && parent != parent.parent_path()) {
            directories_.insert(normalize(parent));
            parent = parent.parent_path();
        }
    }

    void addDirectory(const std::filesystem::path& dirPath) {
        directories_.insert(normalize(dirPath));
        auto parent = dirPath.parent_path();
        while (!parent.empty() && parent != parent.parent_path()) {
            directories_.insert(normalize(parent));
            parent = parent.parent_path();
        }
    }

    [[nodiscard]] bool exists(const std::filesystem::path& path) const override {
        const auto norm = normalize(path);
        return files_.contains(norm) || directories_.contains(norm);
    }

    [[nodiscard]] bool isDirectory(const std::filesystem::path& path) const override {
        return directories_.contains(normalize(path));
    }

    [[nodiscard]] std::vector<FileEntry> listDirectory(const std::filesystem::path& path) const override {
        listCount_++;
        std::vector<FileEntry> result;
        const auto norm = normalize(path);

        // Find immediate subdirectories
        std::set<std::string> immediateDirs;
        for (const auto& d : directories_) {
            std::filesystem::path dp(d);
            if (normalize(dp.parent_path()) == norm && normalize(dp) != norm) {
                immediateDirs.insert(d);
            }
        }
        for (const auto& d : immediateDirs) {
            result.push_back(FileEntry{.path = d, .isDirectory = true});
        }

        // Find immediate files
        for (const auto& f : files_) {
            std::filesystem::path fp(f);
            if (normalize(fp.parent_path()) == norm) {
                result.push_back(FileEntry{.path = f, .isDirectory = false});
            }
        }
        return result;
    }

    [[nodiscard]] int getListCount() const noexcept { return listCount_; }
    void resetListCount() noexcept { listCount_ = 0; }

private:
    static std::string normalize(const std::filesystem::path& p) {
        std::string s = p.generic_string();
        while (s.size() > 1 && s.back() == '/') {
            s.pop_back();
        }
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    mutable int listCount_{0};
    std::set<std::string> directories_;
    std::set<std::string> files_;
};

}  // namespace

TEST_CASE("Upscaler - Glob pattern matching", "[upscaler]") {
    SECTION("Exact match with case insensitivity") {
        REQUIRE(matchGlob("nvngx_dlss.dll", "nvngx_dlss.dll"));
        REQUIRE(matchGlob("NVNGX_DLSS.DLL", "nvngx_dlss.dll"));
        REQUIRE(matchGlob("nvngx_dlss.dll", "NVNGX_DLSS.DLL"));
        REQUIRE_FALSE(matchGlob("nvngx_dlssg.dll", "nvngx_dlss.dll"));
        REQUIRE_FALSE(matchGlob("other.dll", "nvngx_dlss.dll"));
    }

    SECTION("Wildcard matching prefix and suffix") {
        REQUIRE(matchGlob("amd_fidelityfx_dx12.dll", "amd_fidelityfx_*.dll"));
        REQUIRE(matchGlob("amd_fidelityfx_vk.dll", "amd_fidelityfx_*.dll"));
        REQUIRE(matchGlob("AMD_FIDELITYFX_DX12.DLL", "amd_fidelityfx_*.dll"));
        REQUIRE(matchGlob("ffx_fsr2_api_dx12_x64.dll", "ffx_fsr2_api_*.dll"));
        REQUIRE(matchGlob("ffx_fsr3_api_dx12_x64.dll", "ffx_fsr3_api_*.dll"));
        REQUIRE_FALSE(matchGlob("ffx_fsr2_api_dx12_x64.dll", "ffx_fsr3_api_*.dll"));
        REQUIRE_FALSE(matchGlob("unrelated.dll", "amd_fidelityfx_*.dll"));
    }

    SECTION("Single character wildcard") {
        REQUIRE(matchGlob("file1.dll", "file?.dll"));
        REQUIRE_FALSE(matchGlob("file12.dll", "file?.dll"));
    }
}

TEST_CASE("Upscaler - Empty game directory detects nothing", "[upscaler]") {
    auto mockFs = std::make_shared<MockFileSystem>();
    mockFs->addFile("C:/Games/CleanGame/bin/CleanGame.exe");
    mockFs->addFile("C:/Games/CleanGame/bin/Engine.dll");

    UpscalerDetector detector(mockFs, 2);
    const auto info = detector.scanDirectory("C:/Games/CleanGame/bin");

    REQUIRE_FALSE(info.anyDetected());
    REQUIRE_FALSE(info.dlss);
    REQUIRE_FALSE(info.dlssFrameGen);
    REQUIRE_FALSE(info.fsr.has_value());
    REQUIRE_FALSE(info.xess);
    REQUIRE(info.formatSummary() == "None");
}

TEST_CASE("Upscaler - NVIDIA DLSS and Frame Generation detection", "[upscaler]") {
    SECTION("DLSS Super Resolution only") {
        auto mockFs = std::make_shared<MockFileSystem>();
        mockFs->addFile("C:/Games/Cyberpunk/bin/x64/Cyberpunk2077.exe");
        mockFs->addFile("C:/Games/Cyberpunk/bin/x64/nvngx_dlss.dll");

        UpscalerDetector detector(mockFs, 2);
        const auto info = detector.scanDirectory("C:/Games/Cyberpunk/bin/x64");

        REQUIRE(info.anyDetected());
        REQUIRE(info.dlss);
        REQUIRE_FALSE(info.dlssFrameGen);
        REQUIRE_FALSE(info.fsr.has_value());
        REQUIRE_FALSE(info.xess);
        REQUIRE(info.formatSummary() == "DLSS");
    }

    SECTION("DLSS Super Resolution + DLSS Frame Generation") {
        auto mockFs = std::make_shared<MockFileSystem>();
        mockFs->addFile("C:/Games/Cyberpunk/bin/x64/Cyberpunk2077.exe");
        mockFs->addFile("C:/Games/Cyberpunk/bin/x64/nvngx_dlss.dll");
        mockFs->addFile("C:/Games/Cyberpunk/bin/x64/nvngx_dlssg.dll");

        UpscalerDetector detector(mockFs, 2);
        const auto info = detector.scanDirectory("C:/Games/Cyberpunk/bin/x64");

        REQUIRE(info.anyDetected());
        REQUIRE(info.dlss);
        REQUIRE(info.dlssFrameGen);
        REQUIRE_FALSE(info.fsr.has_value());
        REQUIRE_FALSE(info.xess);
        REQUIRE(info.formatSummary() == "DLSS, DLSS-FG");
    }
}

TEST_CASE("Upscaler - AMD FSR version identification", "[upscaler]") {
    SECTION("FSR 2 detection") {
        auto mockFs = std::make_shared<MockFileSystem>();
        mockFs->addFile("C:/Games/Witcher3/bin/x64/witcher3.exe");
        mockFs->addFile("C:/Games/Witcher3/bin/x64/ffx_fsr2_api_dx12_x64.dll");

        UpscalerDetector detector(mockFs, 2);
        const auto info = detector.scanDirectory("C:/Games/Witcher3/bin/x64");

        REQUIRE(info.anyDetected());
        REQUIRE_FALSE(info.dlss);
        REQUIRE(info.fsr.has_value());
        REQUIRE(*info.fsr == "FSR 2");
        REQUIRE(info.formatSummary() == "FSR 2");
    }

    SECTION("FSR 3 detection") {
        auto mockFs = std::make_shared<MockFileSystem>();
        mockFs->addFile("C:/Games/Avatar/bin/x64/game.exe");
        mockFs->addFile("C:/Games/Avatar/bin/x64/ffx_fsr3_api_dx12_x64.dll");

        UpscalerDetector detector(mockFs, 2);
        const auto info = detector.scanDirectory("C:/Games/Avatar/bin/x64");

        REQUIRE(info.anyDetected());
        REQUIRE(info.fsr.has_value());
        REQUIRE(*info.fsr == "FSR 3");
        REQUIRE(info.formatSummary() == "FSR 3");
    }

    SECTION("FidelityFX SDK generic detection") {
        auto mockFs = std::make_shared<MockFileSystem>();
        mockFs->addFile("C:/Games/RDR2/RDR2.exe");
        mockFs->addFile("C:/Games/RDR2/amd_fidelityfx_dx12.dll");

        UpscalerDetector detector(mockFs, 2);
        const auto info = detector.scanDirectory("C:/Games/RDR2");

        REQUIRE(info.anyDetected());
        REQUIRE(info.fsr.has_value());
        REQUIRE(*info.fsr == "FSR");
    }
}

TEST_CASE("Upscaler - Intel XeSS detection", "[upscaler]") {
    auto mockFs = std::make_shared<MockFileSystem>();
    mockFs->addFile("C:/Games/ShadowOfTheTombRaider/SOTTR.exe");
    mockFs->addFile("C:/Games/ShadowOfTheTombRaider/libxess.dll");

    UpscalerDetector detector(mockFs, 2);
    const auto info = detector.scanDirectory("C:/Games/ShadowOfTheTombRaider");

    REQUIRE(info.anyDetected());
    REQUIRE_FALSE(info.dlss);
    REQUIRE(info.xess);
    REQUIRE(info.formatSummary() == "XeSS");
}

TEST_CASE("Upscaler - Multi-technology combination summary", "[upscaler]") {
    auto mockFs = std::make_shared<MockFileSystem>();
    mockFs->addFile("C:/Games/Starfield/Starfield.exe");
    mockFs->addFile("C:/Games/Starfield/nvngx_dlss.dll");
    mockFs->addFile("C:/Games/Starfield/nvngx_dlssg.dll");
    mockFs->addFile("C:/Games/Starfield/ffx_fsr3_api_dx12_x64.dll");
    mockFs->addFile("C:/Games/Starfield/libxess.dll");

    UpscalerDetector detector(mockFs, 2);
    const auto info = detector.scanDirectory("C:/Games/Starfield");

    REQUIRE(info.dlss);
    REQUIRE(info.dlssFrameGen);
    REQUIRE(info.fsr.has_value());
    REQUIRE(*info.fsr == "FSR 3");
    REQUIRE(info.xess);
    REQUIRE(info.formatSummary() == "DLSS, DLSS-FG, FSR 3, XeSS");
}

TEST_CASE("Upscaler - Subdirectory search depth restriction", "[upscaler]") {
    auto mockFs = std::make_shared<MockFileSystem>();
    // Root = C:/Games/UnrealGame/Binaries/Win64
    mockFs->addFile("C:/Games/UnrealGame/Binaries/Win64/UnrealGame.exe");

    // Depth 1: plugins/nvngx_dlss.dll
    mockFs->addFile("C:/Games/UnrealGame/Binaries/Win64/plugins/nvngx_dlss.dll");

    // Depth 2: plugins/amd/ffx_fsr2_api_dx12_x64.dll
    mockFs->addFile("C:/Games/UnrealGame/Binaries/Win64/plugins/amd/ffx_fsr2_api_dx12_x64.dll");

    // Depth 3: plugins/amd/deep/libxess.dll (exceeds maxDepth = 2)
    mockFs->addFile("C:/Games/UnrealGame/Binaries/Win64/plugins/amd/deep/libxess.dll");

    SECTION("Depth 2 finds depth 1 and depth 2, but ignores depth 3") {
        UpscalerDetector detector(mockFs, 2);
        const auto info = detector.scanDirectory("C:/Games/UnrealGame/Binaries/Win64");

        REQUIRE(info.dlss);             // Found at depth 1
        REQUIRE(info.fsr.has_value());  // Found at depth 2
        REQUIRE_FALSE(info.xess);       // Depth 3 was safely ignored due to depth limit
    }

    SECTION("Depth 0 inspects only the root directory") {
        UpscalerDetector detector(mockFs, 0);
        const auto info = detector.scanDirectory("C:/Games/UnrealGame/Binaries/Win64");

        REQUIRE_FALSE(info.dlss);
        REQUIRE_FALSE(info.fsr.has_value());
        REQUIRE_FALSE(info.xess);
    }
}

TEST_CASE("Upscaler - Caching eliminates redundant disk scans", "[upscaler]") {
    auto mockFs = std::make_shared<MockFileSystem>();
    mockFs->addFile("C:/Games/GameA/GameA.exe");
    mockFs->addFile("C:/Games/GameA/nvngx_dlss.dll");

    UpscalerDetector detector(mockFs, 2);

    // Initial query triggers scan
    const auto info1 = detector.getUpscalerInfo(1001, "C:/Games/GameA/GameA.exe");
    REQUIRE(info1.has_value());
    REQUIRE(info1->dlss);
    const int scansAfterFirstCall = mockFs->getListCount();
    REQUIRE(scansAfterFirstCall > 0);

    // Subsequent calls with same PID/path return cached result without scanning
    const auto info2 = detector.getUpscalerInfo(1001, "C:/Games/GameA/GameA.exe");
    REQUIRE(info2.has_value());
    REQUIRE(info2->dlss);
    REQUIRE(mockFs->getListCount() == scansAfterFirstCall);

    // Subsequent call with same PID and empty path uses PID cache
    const auto info3 = detector.getUpscalerInfo(1001, "");
    REQUIRE(info3.has_value());
    REQUIRE(info3->dlss);
    REQUIRE(mockFs->getListCount() == scansAfterFirstCall);

    // Clear cache forces rescan
    detector.clearCache();
    const auto info4 = detector.getUpscalerInfo(1001, "C:/Games/GameA/GameA.exe");
    REQUIRE(info4.has_value());
    REQUIRE(mockFs->getListCount() > scansAfterFirstCall);
}

TEST_CASE("Upscaler - Extensible rules allow adding custom library definitions", "[upscaler]") {
    auto mockFs = std::make_shared<MockFileSystem>();
    mockFs->addFile("C:/Games/ModdedGame/ModdedGame.exe");
    mockFs->addFile("C:/Games/ModdedGame/custom_dlss_wrapper.dll");

    UpscalerDetector detector(mockFs, 2);

    // Before custom rule
    const auto initial = detector.scanDirectory("C:/Games/ModdedGame");
    REQUIRE_FALSE(initial.dlss);

    // Add custom rule
    detector.addRule(
        KnownDllRule{.pattern = "custom_dlss_wrapper.dll", .type = UpscalerType::Dlss, .versionTag = "Custom"});

    const auto after = detector.scanDirectory("C:/Games/ModdedGame");
    REQUIRE(after.dlss);
    REQUIRE(after.formatSummary() == "DLSS");
}

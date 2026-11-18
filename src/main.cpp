#include "capture/etw_present_capture.hpp"
#include "capture/foreground_tracker.hpp"
#include "core/metrics_hub.hpp"
#include "sensors/sensor_manager.hpp"
#include "ui/overlay_window.hpp"
#include "upscaler/upscaler_detector.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    bool debugMode = false;
    bool headlessMode = false;
    int maxFrames = -1; // -1 runs indefinitely

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--debug" || arg == "-d") {
            debugMode = true;
        } else if (arg == "--headless") {
            headlessMode = true;
        } else if (arg == "-n" && i + 1 < argc) {
            maxFrames = std::stoi(argv[++i]);
        }
    }

    if (debugMode) {
        std::cout << "========================================================\n";
        std::cout << " perfoverlay v0.1 - External Overlay (Debug Console)\n";
        std::cout << "========================================================\n";
    }

    // 1. Initialize hardware sensor subsystem
    perfoverlay::sensors::SensorManager sensorManager;
    sensorManager.init();
    sensorManager.start();

    // 2. Initialize real-time ETW present capture (No hooking/injection)
    perfoverlay::capture::EtwPresentCapture etwCapture;
    const auto etwRes = etwCapture.start();
    if (!etwRes.ok() && debugMode) {
        std::cerr << "[ETW Notice] Real-time frame capture not active: " << etwRes.errorMessage << "\n";
    }

    // 3. Foreground process tracker
    perfoverlay::capture::ForegroundProcessTracker fgTracker;

    // 4. Upscaler detector (file-based game directory scanner, anti-cheat safe)
    perfoverlay::upscaler::UpscalerDetector upscalerDetector;

    // 5. Central MetricsHub coordination
    perfoverlay::core::MetricsHub hub(&fgTracker, &etwCapture, &sensorManager, &upscalerDetector);

    // 6. Initialize Win32 + D3D11 + ImGui Overlay Window
    perfoverlay::ui::OverlayWindow overlay;
    if (!headlessMode) {
        if (!overlay.init()) {
            std::cerr << "[Error] Failed to initialize DirectX 11 transparent overlay window.\n";
            return 1;
        }

        // Render initial frame with dummy data to verify rendering pipeline
        perfoverlay::core::MetricsSnapshot initialDummy;
        initialDummy.hasActiveGame = true;
        initialDummy.activeGame = perfoverlay::core::ActiveGameInfo{
            .processId = 0,
            .processName = "Waiting for game...",
            .windowTitle = "perfoverlay"
        };
        initialDummy.frameStats = perfoverlay::core::FrameStats{
            .avgFps = 0.0,
            .currentFps = 0.0,
            .minFps = 0.0,
            .maxFps = 0.0,
            .p1LowFps = 0.0,
            .p01LowFps = 0.0,
            .avgFrameTimeMs = 0.0,
            .p99FrameTimeMs = 0.0
        };
        initialDummy.upscaler = perfoverlay::upscaler::UpscalerInfo{
            .dlss = true,
            .dlssFrameGen = true,
            .fsr = "FSR 3",
            .xess = false
        };
        overlay.render(initialDummy);
    }

    // Connect and start MetricsHub background worker
    hub.start();

    if (debugMode) {
        std::cout << "[Overlay] Running. Press F10 to toggle visibility. Press Ctrl+C to exit.\n";
    }

    // Main application render and message loop
    int frameCount = 0;
    auto lastDebugLogTime = std::chrono::steady_clock::now();

    while (maxFrames < 0 || frameCount < maxFrames) {
        if (!headlessMode) {
            if (!overlay.pollMessages()) {
                break; // WM_QUIT received
            }

            const auto snapshot = hub.getSnapshot();
            overlay.render(snapshot);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        ++frameCount;

        if (debugMode) {
            const auto now = std::chrono::steady_clock::now();
            if (now - lastDebugLogTime >= std::chrono::seconds(1)) {
                lastDebugLogTime = now;
                const auto snap = hub.getSnapshot();

                std::cout << "[Debug Frame #" << frameCount << "] ";
                if (snap.hasActiveGame && snap.activeGame.has_value()) {
                    std::cout << "Game: " << snap.activeGame->processName << " | ";
                    if (snap.frameStats.has_value() && snap.frameStats->avgFps > 0.0) {
                        std::cout << "FPS: " << std::fixed << std::setprecision(1) << snap.frameStats->currentFps
                                  << " (1% Low: " << snap.frameStats->p1LowFps << ") | ";
                    } else {
                        std::cout << "FPS: N/A | ";
                    }
                } else {
                    std::cout << "Game: [None/Desktop] | ";
                }

                if (snap.sensors.cpuUsagePercent.has_value()) {
                    std::cout << "CPU: " << std::fixed << std::setprecision(1) << *snap.sensors.cpuUsagePercent << "% | ";
                }
                if (snap.sensors.gpuUsagePercent.has_value()) {
                    std::cout << "GPU: " << std::fixed << std::setprecision(1) << *snap.sensors.gpuUsagePercent << "%";
                }
                if (snap.upscaler.has_value() && snap.upscaler->anyDetected()) {
                    std::cout << " | Upscaler: " << snap.upscaler->formatSummary();
                }
                std::cout << "\n";
            }
        }
    }

    if (!headlessMode) {
        overlay.shutdown();
    }
    hub.stop();
    etwCapture.stop();
    sensorManager.stop();

    return 0;
}

#include "capture/etw_present_capture.hpp"
#include "capture/foreground_tracker.hpp"
#include "core/metrics_hub.hpp"
#include "sensors/sensor_manager.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    std::cout << "========================================================\n";
    std::cout << " perfoverlay v0.1 - Unified MetricsHub Telemetry\n";
    std::cout << "========================================================\n";

    int maxIterations = -1; // -1 runs indefinitely until Ctrl+C
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-n" && i + 1 < argc) {
            maxIterations = std::stoi(argv[++i]);
        } else if (std::string(argv[i]) == "--once") {
            maxIterations = 1;
        }
    }

    // 1. Initialize sensor subsystem
    perfoverlay::sensors::SensorManager sensorManager;
    sensorManager.init();
    sensorManager.start();

    // 2. Initialize ETW present capture (No hooking/injection)
    perfoverlay::capture::EtwPresentCapture etwCapture;
    const auto etwRes = etwCapture.start();
    if (!etwRes.ok()) {
        std::cerr << "[ETW Notice] Real-time frame capture not active: " << etwRes.errorMessage << "\n";
    }

    // 3. Foreground process tracker
    perfoverlay::capture::ForegroundProcessTracker fgTracker;

    // 4. Unified MetricsHub coordination
    perfoverlay::core::MetricsHub hub(&fgTracker, &etwCapture, &sensorManager);
    hub.start();

    std::cout << "MetricsHub running (FPS: 4x/s, Sensors: 500 ms). Outputting every 1 s...\n";
    if (maxIterations < 0) {
        std::cout << "Press Ctrl+C to stop.\n\n";
    }

    int count = 0;
    while (maxIterations < 0 || count < maxIterations) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++count;

        const auto snap = hub.getSnapshot();

        std::cout << "--------------------------------------------------------\n";
        std::cout << "[#" << std::setw(2) << count << "] ";

        if (snap.hasActiveGame && snap.activeGame.has_value()) {
            std::cout << "Game: " << snap.activeGame->processName
                      << " (PID: " << snap.activeGame->processId << ") - \""
                      << snap.activeGame->windowTitle << "\"\n";

            if (snap.frameStats.has_value() && snap.frameStats->avgFps > 0.0) {
                const auto& f = *snap.frameStats;
                std::cout << "     FPS: " << std::fixed << std::setprecision(1) << f.currentFps
                          << " | Avg: " << f.avgFps
                          << " | 1% Low: " << f.p1LowFps
                          << " | 0.1% Low: " << f.p01LowFps << "\n";
                std::cout << "     FrameTime: " << std::fixed << std::setprecision(2) << f.avgFrameTimeMs
                          << " ms | p99: " << f.p99FrameTimeMs << " ms\n";
            } else {
                std::cout << "     FPS: (waiting for graphics present events...)\n";
            }
        } else {
            std::cout << "Game: [No active game window focused]\n";
        }

        // Hardware sensor telemetry
        std::cout << "     HW:  ";
        if (snap.sensors.cpuUsagePercent.has_value()) {
            std::cout << "CPU: " << std::fixed << std::setprecision(1) << *snap.sensors.cpuUsagePercent << "% | ";
        } else {
            std::cout << "CPU: N/A | ";
        }

        if (snap.sensors.gpuUsagePercent.has_value()) {
            std::cout << "GPU: " << std::fixed << std::setprecision(1) << *snap.sensors.gpuUsagePercent << "% ";
        } else {
            std::cout << "GPU: N/A ";
        }

        if (snap.sensors.gpuTempC.has_value()) {
            std::cout << "(" << std::fixed << std::setprecision(1) << *snap.sensors.gpuTempC << " C) | ";
        } else {
            std::cout << "| ";
        }

        if (snap.sensors.gpuVramUsedMB.has_value() && snap.sensors.gpuVramTotalMB.has_value()) {
            std::cout << "VRAM: " << std::fixed << std::setprecision(0) << *snap.sensors.gpuVramUsedMB << "/"
                      << *snap.sensors.gpuVramTotalMB << " MB | ";
        } else if (snap.sensors.gpuVramUsedMB.has_value()) {
            std::cout << "VRAM: " << std::fixed << std::setprecision(0) << *snap.sensors.gpuVramUsedMB << " MB | ";
        }

        if (snap.sensors.gpuClockMHz.has_value()) {
            std::cout << "Clock: " << std::fixed << std::setprecision(0) << *snap.sensors.gpuClockMHz << " MHz | ";
        }

        if (snap.sensors.gpuPowerW.has_value()) {
            std::cout << "Power: " << std::fixed << std::setprecision(1) << *snap.sensors.gpuPowerW << " W";
        }
        std::cout << "\n";
    }

    hub.stop();
    etwCapture.stop();
    sensorManager.stop();
    return 0;
}

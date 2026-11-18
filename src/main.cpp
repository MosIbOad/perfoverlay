#include "capture/etw_present_capture.hpp"
#include "capture/foreground_tracker.hpp"
#include "sensors/sensor_manager.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    std::cout << "========================================================\n";
    std::cout << " perfoverlay v0.1 - Real-Time ETW & Hardware Monitor\n";
    std::cout << "========================================================\n";

    int maxIterations = -1; // -1 runs indefinitely until Ctrl+C
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-n" && i + 1 < argc) {
            maxIterations = std::stoi(argv[++i]);
        } else if (std::string(argv[i]) == "--once") {
            maxIterations = 1;
        }
    }

    // 1. Initialize and start hardware sensor polling
    perfoverlay::sensors::SensorManager sensorManager;
    if (!sensorManager.init()) {
        std::cerr << "[Warning] Hardware sensors could not be initialized.\n";
    }
    sensorManager.start();

    // 2. Initialize and start ETW present capture (No hooking/injection)
    perfoverlay::capture::EtwPresentCapture etwCapture;
    const auto etwResult = etwCapture.start();
    if (!etwResult.ok()) {
        std::cerr << "\n[ETW Notice] Real-time frame capture could not be started:\n"
                  << etwResult.errorMessage << "\n\n";
    } else {
        std::cout << "[ETW] Real-time session started (DXGI & DxgKrnl providers active).\n";
    }

    perfoverlay::capture::ForegroundProcessTracker fgTracker;

    std::cout << "Monitoring active foreground window. Printing telemetry every 1 s...\n";
    if (maxIterations < 0) {
        std::cout << "Press Ctrl+C to stop.\n\n";
    }

    int count = 0;
    while (maxIterations < 0 || count < maxIterations) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++count;

        std::cout << "--------------------------------------------------------\n";
        std::cout << "[#" << std::setw(2) << count << "] ";

        // Query active foreground game/application
        const auto activeFg = fgTracker.getActiveProcess();
        if (activeFg.has_value()) {
            std::cout << "Active: " << activeFg->processName
                      << " (PID: " << activeFg->processId << ") - \""
                      << activeFg->windowTitle << "\"\n";

            // Query frame stats for active process
            const auto stats = etwCapture.getProcessStats(activeFg->processId);
            if (stats.has_value() && stats->avgFps > 0.0) {
                std::cout << "     FPS: " << std::fixed << std::setprecision(1) << stats->currentFps
                          << " | Avg: " << stats->avgFps
                          << " | 1% Low: " << stats->p1LowFps
                          << " | 0.1% Low: " << stats->p01LowFps << "\n";
                std::cout << "     FrameTime: " << std::fixed << std::setprecision(2) << stats->avgFrameTimeMs
                          << " ms | p99: " << stats->p99FrameTimeMs << " ms\n";
            } else {
                std::cout << "     FPS: (waiting for graphics present frames...)\n";
            }
        } else {
            std::cout << "Active: None / Desktop / Explorer (waiting for game window...)\n";
        }

        // Hardware sensor telemetry
        const auto snap = sensorManager.getSnapshot();
        std::cout << "     HW:  ";
        if (snap.cpuUsagePercent.has_value()) {
            std::cout << "CPU: " << std::fixed << std::setprecision(1) << *snap.cpuUsagePercent << "% | ";
        } else {
            std::cout << "CPU: N/A | ";
        }

        if (snap.gpuUsagePercent.has_value()) {
            std::cout << "GPU: " << std::fixed << std::setprecision(1) << *snap.gpuUsagePercent << "% ";
        } else {
            std::cout << "GPU: N/A ";
        }

        if (snap.gpuTempC.has_value()) {
            std::cout << "(" << std::fixed << std::setprecision(1) << *snap.gpuTempC << " C) | ";
        } else {
            std::cout << "| ";
        }

        if (snap.gpuVramUsedMB.has_value() && snap.gpuVramTotalMB.has_value()) {
            std::cout << "VRAM: " << std::fixed << std::setprecision(0) << *snap.gpuVramUsedMB << "/"
                      << *snap.gpuVramTotalMB << " MB | ";
        } else if (snap.gpuVramUsedMB.has_value()) {
            std::cout << "VRAM: " << std::fixed << std::setprecision(0) << *snap.gpuVramUsedMB << " MB | ";
        }

        if (snap.gpuClockMHz.has_value()) {
            std::cout << "Clock: " << std::fixed << std::setprecision(0) << *snap.gpuClockMHz << " MHz | ";
        }

        if (snap.gpuPowerW.has_value()) {
            std::cout << "Power: " << std::fixed << std::setprecision(1) << *snap.gpuPowerW << " W";
        }
        std::cout << "\n";
    }

    etwCapture.stop();
    sensorManager.stop();
    return 0;
}

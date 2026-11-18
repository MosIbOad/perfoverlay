#include "sensors/sensor_manager.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << " perfoverlay v0.1 - Hardware Telemetry\n";
    std::cout << "========================================\n";

    int maxIterations = -1; // -1 runs indefinitely until Ctrl+C
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-n" && i + 1 < argc) {
            maxIterations = std::stoi(argv[++i]);
        } else if (std::string(argv[i]) == "--once") {
            maxIterations = 1;
        }
    }

    perfoverlay::sensors::SensorManager manager;
    if (!manager.init()) {
        std::cerr << "[Warning] No hardware sensors could be initialized.\n";
    }

    manager.start();
    std::cout << "SensorManager started (polling every 500 ms). Outputting snapshot every 1 s...\n";
    if (maxIterations < 0) {
        std::cout << "Press Ctrl+C to stop.\n\n";
    }

    int count = 0;
    while (maxIterations < 0 || count < maxIterations) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++count;

        const auto snap = manager.getSnapshot();

        std::cout << "[" << std::setw(2) << count << "] ";

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

    manager.stop();
    return 0;
}

#include "core/frame_stats.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace perfoverlay::core;

TEST_CASE("FrameTimeBuffer - Empty buffer statistics", "[core][stats]") {
    FrameTimeBuffer buffer(100);

    REQUIRE(buffer.empty());
    REQUIRE(buffer.size() == 0);
    REQUIRE(buffer.front() == 0.0);
    REQUIRE(buffer.back() == 0.0);

    const FrameStats stats = computeStats(buffer);

    REQUIRE(stats.avgFps == 0.0);
    REQUIRE(stats.currentFps == 0.0);
    REQUIRE(stats.minFps == 0.0);
    REQUIRE(stats.maxFps == 0.0);
    REQUIRE(stats.p1LowFps == 0.0);
    REQUIRE(stats.p01LowFps == 0.0);
    REQUIRE(stats.avgFrameTimeMs == 0.0);
    REQUIRE(stats.p99FrameTimeMs == 0.0);
}

TEST_CASE("FrameTimeBuffer - Single sample statistics", "[core][stats]") {
    FrameTimeBuffer buffer(100);
    buffer.push(16.0); // 16.0 ms = 62.5 FPS

    REQUIRE_FALSE(buffer.empty());
    REQUIRE(buffer.size() == 1);
    REQUIRE(buffer.front() == 16.0);
    REQUIRE(buffer.back() == 16.0);

    const FrameStats stats = computeStats(buffer);

    REQUIRE(stats.avgFps == Approx(62.5));
    REQUIRE(stats.currentFps == Approx(62.5));
    REQUIRE(stats.minFps == Approx(62.5));
    REQUIRE(stats.maxFps == Approx(62.5));
    REQUIRE(stats.p1LowFps == Approx(62.5));
    REQUIRE(stats.p01LowFps == Approx(62.5));
    REQUIRE(stats.avgFrameTimeMs == Approx(16.0));
    REQUIRE(stats.p99FrameTimeMs == Approx(16.0));
}

TEST_CASE("FrameTimeBuffer - Constant 60 FPS stream", "[core][stats]") {
    FrameTimeBuffer buffer(200);
    constexpr double frame60Ms = 1000.0 / 60.0; // 16.666666... ms

    for (int i = 0; i < 120; ++i) {
        buffer.push(frame60Ms);
    }

    REQUIRE(buffer.size() == 120);

    const FrameStats stats = computeStats(buffer);

    REQUIRE(stats.avgFps == Approx(60.0));
    REQUIRE(stats.currentFps == Approx(60.0));
    REQUIRE(stats.minFps == Approx(60.0));
    REQUIRE(stats.maxFps == Approx(60.0));
    REQUIRE(stats.p1LowFps == Approx(60.0));
    REQUIRE(stats.p01LowFps == Approx(60.0));
    REQUIRE(stats.avgFrameTimeMs == Approx(frame60Ms));
    REQUIRE(stats.p99FrameTimeMs == Approx(frame60Ms));
}

TEST_CASE("FrameTimeBuffer - Capacity overflow drops oldest entries", "[core][buffer]") {
    FrameTimeBuffer buffer(5); // Capacity 5

    for (int i = 1; i <= 5; ++i) {
        buffer.push(static_cast<double>(i * 10)); // 10, 20, 30, 40, 50
    }

    REQUIRE(buffer.size() == 5);
    REQUIRE(buffer.front() == 10.0);
    REQUIRE(buffer.back() == 50.0);

    // Push two more items to trigger capacity overflow
    buffer.push(60.0);
    buffer.push(70.0);

    REQUIRE(buffer.size() == 5);
    REQUIRE(buffer.front() == 30.0); // 10 and 20 were dropped
    REQUIRE(buffer.back() == 70.0);

    REQUIRE(buffer[0] == 30.0);
    REQUIRE(buffer[1] == 40.0);
    REQUIRE(buffer[2] == 50.0);
    REQUIRE(buffer[3] == 60.0);
    REQUIRE(buffer[4] == 70.0);
}

TEST_CASE("FrameTimeBuffer - Timestamp-based pruning", "[core][buffer]") {
    using namespace std::chrono_literals;
    FrameTimeBuffer buffer(100, 2s); // Max 2 seconds window

    const auto t0 = std::chrono::steady_clock::time_point{100s};

    buffer.push(16.0, t0);
    buffer.push(16.0, t0 + 1s);
    REQUIRE(buffer.size() == 2);

    // Push frame at t0 + 2.5s (exceeds 2s window from t0)
    buffer.push(16.0, t0 + 2500ms);

    // Entry at t0 is discarded because 2.5s - 2.0s = 0.5s > 0s (t0)
    REQUIRE(buffer.size() == 2);
    REQUIRE(buffer.entryAt(0).timestamp == t0 + 1s);
    REQUIRE(buffer.entryAt(1).timestamp == t0 + 2500ms);
}

TEST_CASE("FrameStats - Periodic spike data", "[core][stats]") {
    FrameTimeBuffer buffer(100);

    // 100 frames total:
    // 90 frames of 10.0 ms (100 FPS)
    // 10 frames (every 10th frame) of 20.0 ms (50 FPS)
    for (int i = 1; i <= 100; ++i) {
        if (i % 10 == 0) {
            buffer.push(20.0);
        } else {
            buffer.push(10.0);
        }
    }

    REQUIRE(buffer.size() == 100);

    const FrameStats stats = computeStats(buffer);

    // Hand-calculated expected values:
    // Total frame time = 90 * 10.0 + 10 * 20.0 = 1100.0 ms
    // avgFrameTimeMs = 1100.0 / 100 = 11.0 ms
    // avgFps = 1000.0 / 11.0 = 90.9090909... FPS
    REQUIRE(stats.avgFrameTimeMs == Approx(11.0));
    REQUIRE(stats.avgFps == Approx(1000.0 / 11.0));

    // Last frame pushed was i=100 (multiple of 10), so last frame time is 20.0 ms -> currentFps = 50.0
    REQUIRE(stats.currentFps == Approx(50.0));

    // Min FPS is from max frame time (20.0 ms) -> 50.0 FPS
    REQUIRE(stats.minFps == Approx(50.0));

    // Max FPS is from min frame time (10.0 ms) -> 100.0 FPS
    REQUIRE(stats.maxFps == Approx(100.0));

    // Slowest 1% (k = ceil(100 * 0.01) = 1 frame):
    // Slowest frame is 20.0 ms -> p1LowFps = 1000.0 / 20.0 = 50.0 FPS
    REQUIRE(stats.p1LowFps == Approx(50.0));

    // Slowest 0.1% (k = ceil(100 * 0.001) = 1 frame):
    // Slowest frame is 20.0 ms -> p01LowFps = 1000.0 / 20.0 = 50.0 FPS
    REQUIRE(stats.p01LowFps == Approx(50.0));

    // 99th percentile frame time:
    // Sorted array of 100 elements has 90x 10.0 ms followed by 10x 20.0 ms.
    // Index ceil(0.99 * 100) - 1 = 98 is one of the 20.0 ms frames.
    REQUIRE(stats.p99FrameTimeMs == Approx(20.0));
}

TEST_CASE("FrameStats - Hand-calculated 1% Low and 0.1% Low with distinct values", "[core][stats]") {
    FrameTimeBuffer buffer(1500);

    // 1000 frames total:
    // - 989 frames of 10.0 ms (100 FPS)
    // - 10 frames of 25.0 ms (40 FPS)
    // - 1 frame of 50.0 ms (20 FPS)
    for (int i = 0; i < 989; ++i) {
        buffer.push(10.0);
    }
    for (int i = 0; i < 10; ++i) {
        buffer.push(25.0);
    }
    buffer.push(50.0); // Latest frame is the 50ms spike

    REQUIRE(buffer.size() == 1000);

    std::vector<double> scratch;
    const FrameStats stats = computeStats(buffer, scratch);

    // Hand calculations:
    // Total ms = 989 * 10.0 + 10 * 25.0 + 1 * 50.0 = 9890 + 250 + 50 = 10190.0 ms
    // avgFrameTimeMs = 10190.0 / 1000 = 10.19 ms
    // avgFps = 1000.0 / 10.19 = 98.135426889... FPS
    REQUIRE(stats.avgFrameTimeMs == Approx(10.19));
    REQUIRE(stats.avgFps == Approx(1000.0 / 10.19));

    // Current FPS: last frame is 50.0 ms -> 20.0 FPS
    REQUIRE(stats.currentFps == Approx(20.0));

    // Min FPS: slowest frame is 50.0 ms -> 20.0 FPS
    REQUIRE(stats.minFps == Approx(20.0));

    // Max FPS: fastest frame is 10.0 ms -> 100.0 FPS
    REQUIRE(stats.maxFps == Approx(100.0));

    // 0.1% Low calculation:
    // k01 = ceil(1000 * 0.001) = 1 frame.
    // The single worst frame is 50.0 ms.
    // worst01AvgMs = 50.0 ms.
    // p01LowFps = 1000.0 / 50.0 = 20.0 FPS.
    REQUIRE(stats.p01LowFps == Approx(20.0));

    // 1% Low calculation:
    // k1 = ceil(1000 * 0.01) = 10 frames.
    // The 10 slowest frames are: one 50.0 ms frame and nine 25.0 ms frames.
    // worst1Sum = 50.0 + (9 * 25.0) = 50.0 + 225.0 = 275.0 ms.
    // worst1AvgMs = 275.0 / 10 = 27.5 ms.
    // p1LowFps = 1000.0 / 27.5 = 36.36363636... FPS (400 / 11 FPS).
    // Note: If using percentile threshold, rank 990 would be 25.0 ms (40.0 FPS).
    // The average-of-worst-1% method correctly penalizes the 50ms stutter!
    REQUIRE(stats.p1LowFps == Approx(1000.0 / 27.5));

    // 99th percentile frame time:
    // Index ceil(0.99 * 1000) - 1 = 989.
    // Sorted indices: 0..988 are 10.0ms, 989..998 are 25.0ms, 999 is 50.0ms.
    // Index 989 is 25.0 ms.
    REQUIRE(stats.p99FrameTimeMs == Approx(25.0));
}

#include "capture/etw_types.hpp"
#include "capture/foreground_tracker.hpp"
#include "capture/frame_time_calculator.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace perfoverlay::capture;

TEST_CASE("FrameTimeCalculator - Basic delta calculations", "[capture][calculator]") {
    // 10 MHz QPC clock (10,000,000 ticks per second -> 10,000 ticks per ms)
    constexpr uint64_t qpcFreq = 10'000'000;
    FrameTimeCalculator calc(qpcFreq, 0.1);

    const uint32_t pid = 1234;

    // First frame establishes baseline; must return nullopt
    auto f1 = calc.processEvent(pid, 100'000'000);
    REQUIRE_FALSE(f1.has_value());
    REQUIRE(calc.trackedProcessCount() == 1);

    // Frame 2: +166,667 ticks (~16.6667 ms = ~60 FPS)
    auto f2 = calc.processEvent(pid, 100'166'667);
    REQUIRE(f2.has_value());
    REQUIRE(*f2 == Approx(16.6667).epsilon(0.001));

    // Frame 3: +83,333 ticks (~8.3333 ms = ~120 FPS)
    auto f3 = calc.processEvent(pid, 100'250'000);
    REQUIRE(f3.has_value());
    REQUIRE(*f3 == Approx(8.3333).epsilon(0.001));
}

TEST_CASE("FrameTimeCalculator - Sub-millisecond duplicate filtering", "[capture][calculator]") {
    constexpr uint64_t qpcFreq = 10'000'000;
    FrameTimeCalculator calc(qpcFreq, 0.1);  // minDelta = 0.1 ms (1000 ticks)

    const uint32_t pid = 5555;

    calc.processEvent(pid, 1'000'000);

    // Second event arrives 200 ticks later (0.02 ms delta, nested DxgKrnl event)
    auto dup = calc.processEvent(pid, 1'000'200);
    REQUIRE_FALSE(dup.has_value());  // Must be filtered out

    // Real next frame arrives 166,667 ticks after baseline
    auto nextFrame = calc.processEvent(pid, 1'166'667);
    REQUIRE(nextFrame.has_value());
    REQUIRE(*nextFrame == Approx(16.6667).epsilon(0.001));
}

TEST_CASE("FrameTimeCalculator - Out of order or zero delta handling", "[capture][calculator]") {
    constexpr uint64_t qpcFreq = 10'000'000;
    FrameTimeCalculator calc(qpcFreq, 0.1);

    const uint32_t pid = 4321;

    calc.processEvent(pid, 50'000'000);

    // Identical timestamp
    auto sameTime = calc.processEvent(pid, 50'000'000);
    REQUIRE_FALSE(sameTime.has_value());

    // Earlier timestamp (time went backwards / out of order)
    auto olderTime = calc.processEvent(pid, 49'999'000);
    REQUIRE_FALSE(olderTime.has_value());

    // Gap larger than 1 second (10,000,001 ticks = 1000.0001 ms) re-establishes baseline
    auto longPause = calc.processEvent(pid, 60'000'001);
    REQUIRE_FALSE(longPause.has_value());

    // Next normal frame after pause works properly
    auto afterPause = calc.processEvent(pid, 60'166'667);
    REQUIRE(afterPause.has_value());
    REQUIRE(*afterPause == Approx(16.6667).epsilon(0.001));
}

TEST_CASE("FrameTimeCalculator - Multiple process isolation", "[capture][calculator]") {
    constexpr uint64_t qpcFreq = 10'000'000;
    FrameTimeCalculator calc(qpcFreq, 0.1);

    const uint32_t pidA = 1001;
    const uint32_t pidB = 2002;

    calc.processEvent(pidA, 10'000'000);
    calc.processEvent(pidB, 20'000'000);

    // Interleaved events
    auto a1 = calc.processEvent(pidA, 10'166'667);  // ~16.667 ms
    auto b1 = calc.processEvent(pidB, 20'333'333);  // ~33.333 ms

    REQUIRE(a1.has_value());
    REQUIRE(*a1 == Approx(16.6667).epsilon(0.001));

    REQUIRE(b1.has_value());
    REQUIRE(*b1 == Approx(33.3333).epsilon(0.001));
}

TEST_CASE("ForegroundProcessTracker - Pure exclusion logic", "[capture][foreground]") {
    constexpr uint32_t ownPid = 9999;

    // System processes
    REQUIRE(ForegroundProcessTracker::isExcluded(0, ownPid, "Idle", "System"));
    REQUIRE(ForegroundProcessTracker::isExcluded(4, ownPid, "System", "System"));

    // Own process
    REQUIRE(ForegroundProcessTracker::isExcluded(ownPid, ownPid, "perfoverlay.exe", "perfoverlay"));

    // Shell & explorer
    REQUIRE(ForegroundProcessTracker::isExcluded(1234, ownPid, "explorer.exe", "File Explorer"));
    REQUIRE(ForegroundProcessTracker::isExcluded(1235, ownPid, "EXPLORER.EXE", "Desktop"));
    REQUIRE(ForegroundProcessTracker::isExcluded(2222, ownPid, "dwm.exe", "DWM"));
    REQUIRE(ForegroundProcessTracker::isExcluded(3333, ownPid, "SearchHost.exe", "Search"));
    REQUIRE(ForegroundProcessTracker::isExcluded(4444, ownPid, "TextInputHost.exe", ""));
    REQUIRE(ForegroundProcessTracker::isExcluded(4445, ownPid, "cmd.exe", "Command Prompt"));
    REQUIRE(ForegroundProcessTracker::isExcluded(4446, ownPid, "powershell.exe", "Windows PowerShell"));
    REQUIRE(ForegroundProcessTracker::isExcluded(4447, ownPid, "Taskmgr.exe", "Task Manager"));

    // Empty or special shell titles
    REQUIRE(ForegroundProcessTracker::isExcluded(5555, ownPid, "app.exe", ""));
    REQUIRE(ForegroundProcessTracker::isExcluded(5556, ownPid, "app.exe", "Program Manager"));
    REQUIRE(ForegroundProcessTracker::isExcluded(5557, ownPid, "app.exe", "Taskbar"));

    // Normal game: must NOT be excluded
    REQUIRE_FALSE(ForegroundProcessTracker::isExcluded(7777, ownPid, "Cyberpunk2077.exe", "Cyberpunk 2077"));
    REQUIRE_FALSE(ForegroundProcessTracker::isExcluded(8888, ownPid, "cs2.exe", "Counter-Strike 2"));
}

TEST_CASE("ETW Types - Present event classification", "[capture][etw_types]") {
    // DXGI Present Start (42), Stop (43), MPO Start (55)
    REQUIRE(isPresentStartEvent(DXGI_PROVIDER_GUID, EVENT_ID_DXGI_PRESENT_START));
    REQUIRE(isPresentStartEvent(DXGI_PROVIDER_GUID, EVENT_ID_DXGI_PRESENT_MPO_START));
    REQUIRE_FALSE(isPresentStartEvent(DXGI_PROVIDER_GUID, EVENT_ID_DXGI_PRESENT_STOP));
    REQUIRE(isDxgkOrDxgiPresentEvent(DXGI_PROVIDER_GUID, EVENT_ID_DXGI_PRESENT_START));
    REQUIRE(isDxgkOrDxgiPresentEvent(DXGI_PROVIDER_GUID, EVENT_ID_DXGI_PRESENT_STOP));

    // DxgKrnl Present Start (184), Stop (185)
    REQUIRE(isPresentStartEvent(DXGKRNL_PROVIDER_GUID, EVENT_ID_DXGKRNL_PRESENT_START));
    REQUIRE_FALSE(isPresentStartEvent(DXGKRNL_PROVIDER_GUID, EVENT_ID_DXGKRNL_PRESENT_STOP));
    REQUIRE(isDxgkOrDxgiPresentEvent(DXGKRNL_PROVIDER_GUID, EVENT_ID_DXGKRNL_PRESENT_START));
    REQUIRE(isDxgkOrDxgiPresentEvent(DXGKRNL_PROVIDER_GUID, EVENT_ID_DXGKRNL_PRESENT_STOP));

    // Non-present events
    REQUIRE_FALSE(isPresentStartEvent(DXGI_PROVIDER_GUID, 999));
    REQUIRE_FALSE(isDxgkOrDxgiPresentEvent(DXGI_PROVIDER_GUID, 999));
}

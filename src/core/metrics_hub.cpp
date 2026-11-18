#include "core/metrics_hub.hpp"

namespace perfoverlay::core {

MetricsHub::MetricsHub(
    const IForegroundProcessProvider* fgProvider,
    const IPresentCaptureProvider* captureProvider,
    const ISensorSnapshotProvider* sensorProvider
) : fgProvider_(fgProvider),
    captureProvider_(captureProvider),
    sensorProvider_(sensorProvider) {}

MetricsHub::~MetricsHub() {
    stop();
}

void MetricsHub::start() {
    if (running_.exchange(true)) {
        return;
    }

    // Immediate initial poll
    pollOnce(true);

    worker_ = std::jthread([this](std::stop_token stopToken) {
        size_t tickCount = 0;
        while (!stopToken.stop_requested()) {
            std::unique_lock lock(cvMutex_);
            const bool stopped = cv_.wait_for(lock, std::chrono::milliseconds(250), [&stopToken] {
                return stopToken.stop_requested();
            });

            if (stopped || stopToken.stop_requested()) {
                break;
            }

            ++tickCount;
            // FPS updates every tick (250 ms, 4 times/sec). Sensors update every 2nd tick (500 ms).
            const bool pollSensors = (tickCount % 2 == 0);
            pollOnce(pollSensors);
        }
    });
}

void MetricsHub::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    if (worker_.joinable()) {
        worker_.request_stop();
        cv_.notify_all();
        worker_.join();
    }
}

void MetricsHub::pollOnce(bool pollSensors) {
    MetricsSnapshot next;
    {
        std::lock_guard lock(snapshotMutex_);
        next = snapshot_;
    }

    next.timestamp = std::chrono::steady_clock::now();

    // 1. Check active foreground game & update FPS (250 ms cycle)
    if (fgProvider_) {
        const auto activeFg = fgProvider_->getActiveProcess();
        if (activeFg.has_value()) {
            next.hasActiveGame = true;
            next.activeGame = *activeFg;

            if (captureProvider_) {
                next.frameStats = captureProvider_->getProcessStats(activeFg->processId);
            } else {
                next.frameStats = std::nullopt;
            }
        } else {
            next.hasActiveGame = false;
            next.activeGame = std::nullopt;
            next.frameStats = std::nullopt;
        }
    } else {
        next.hasActiveGame = false;
        next.activeGame = std::nullopt;
        next.frameStats = std::nullopt;
    }

    // 2. Hardware sensors (500 ms cycle)
    if (pollSensors && sensorProvider_) {
        next.sensors = sensorProvider_->getSnapshot();
    }

    {
        std::lock_guard lock(snapshotMutex_);
        snapshot_ = std::move(next);
    }
}

MetricsSnapshot MetricsHub::getSnapshot() const {
    std::lock_guard lock(snapshotMutex_);
    return snapshot_;
}

} // namespace perfoverlay::core

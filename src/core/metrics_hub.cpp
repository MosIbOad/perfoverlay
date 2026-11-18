#include "core/metrics_hub.hpp"

namespace perfoverlay::core {

MetricsHub::MetricsHub(const IForegroundProcessProvider* fgProvider, const IPresentCaptureProvider* captureProvider,
                       const ISensorSnapshotProvider* sensorProvider, IUpscalerProvider* upscalerProvider)
    : fgProvider_(fgProvider), captureProvider_(captureProvider), sensorProvider_(sensorProvider),
      upscalerProvider_(upscalerProvider) {}

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
            const bool stopped =
                cv_.wait_for(lock, std::chrono::milliseconds(250), [&stopToken] { return stopToken.stop_requested(); });

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
            const bool sameGame = (next.hasActiveGame && next.activeGame.has_value() &&
                                   next.activeGame->processId == activeFg->processId);
            next.hasActiveGame = true;
            next.activeGame = *activeFg;

            if (captureProvider_) {
                const auto stats = captureProvider_->getProcessStats(activeFg->processId);
                if (stats.has_value()) {
                    next.frameStats = stats;
                    missedStatsTicks_ = 0;
                } else if (!sameGame) {
                    next.frameStats = std::nullopt;
                    missedStatsTicks_ = 0;
                } else {
                    ++missedStatsTicks_;
                    // Allow up to 6 ticks (1.5s) grace period during ETW buffer flushes before clearing
                    if (missedStatsTicks_ >= 6) {
                        next.frameStats = std::nullopt;
                    }
                }
            } else {
                next.frameStats = std::nullopt;
                missedStatsTicks_ = 0;
            }

            if (upscalerProvider_) {
                next.upscaler = upscalerProvider_->getUpscalerInfo(activeFg->processId, activeFg->executablePath);
            } else {
                next.upscaler = std::nullopt;
            }
        } else {
            next.hasActiveGame = false;
            next.activeGame = std::nullopt;
            next.frameStats = std::nullopt;
            next.upscaler = std::nullopt;
            missedStatsTicks_ = 0;
        }
    } else {
        next.hasActiveGame = false;
        next.activeGame = std::nullopt;
        next.frameStats = std::nullopt;
        next.upscaler = std::nullopt;
        missedStatsTicks_ = 0;
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

}  // namespace perfoverlay::core

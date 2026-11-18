#include "core/metrics_hub.hpp"

namespace perfoverlay::core {

MetricsHub::MetricsHub(const IForegroundProcessProvider* fgProvider, const IPresentCaptureProvider* captureProvider,
                       const ISensorSnapshotProvider* sensorProvider, IUpscalerProvider* upscalerProvider,
                       uint32_t fpsPollIntervalMs, uint32_t sensorPollIntervalMs)
    : fgProvider_(fgProvider), captureProvider_(captureProvider), sensorProvider_(sensorProvider),
      upscalerProvider_(upscalerProvider), fpsPollIntervalMs_(fpsPollIntervalMs < 10 ? 10 : fpsPollIntervalMs),
      sensorPollIntervalMs_(sensorPollIntervalMs < 50 ? 50 : sensorPollIntervalMs) {}

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
        auto lastSensorPoll = std::chrono::steady_clock::now();
        while (!stopToken.stop_requested()) {
            std::unique_lock lock(cvMutex_);
            const auto interval = std::chrono::milliseconds(fpsPollIntervalMs_.load());
            const bool stopped =
                cv_.wait_for(lock, interval, [&stopToken] { return stopToken.stop_requested(); });

            if (stopped || stopToken.stop_requested()) {
                break;
            }

            const auto now = std::chrono::steady_clock::now();
            bool pollSensors = false;
            if (now - lastSensorPoll >= std::chrono::milliseconds(sensorPollIntervalMs_.load())) {
                pollSensors = true;
                lastSensorPoll = now;
            }
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
                    // Allow up to 1.5s grace period during ETW buffer flushes before clearing
                    const uint32_t pollMs = fpsPollIntervalMs_.load();
                    const size_t maxMissed = (1500 / (pollMs > 0 ? pollMs : 100));
                    if (missedStatsTicks_ >= maxMissed) {
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

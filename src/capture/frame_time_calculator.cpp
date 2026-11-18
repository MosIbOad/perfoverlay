#include "capture/frame_time_calculator.hpp"

#include <algorithm>

namespace perfoverlay::capture {

FrameTimeCalculator::FrameTimeCalculator(uint64_t qpcFrequency, double minDeltaMs)
    : qpcFrequency_(std::max<uint64_t>(1, qpcFrequency)), minDeltaMs_(std::max<double>(0.0, minDeltaMs)) {}

std::optional<double> FrameTimeCalculator::processEvent(uint32_t processId, uint64_t qpcTimestamp) {
    if (processId == 0) {
        return std::nullopt;
    }

    auto it = lastTimestamps_.find(processId);
    if (it == lastTimestamps_.end()) {
        // First present event recorded for this process: establishes baseline
        lastTimestamps_.emplace(processId, qpcTimestamp);
        return std::nullopt;
    }

    const uint64_t prevTimestamp = it->second;
    if (qpcTimestamp <= prevTimestamp) {
        // Out of order or identical timestamp: ignore
        return std::nullopt;
    }

    const uint64_t deltaTicks = qpcTimestamp - prevTimestamp;
    const double deltaMs = (static_cast<double>(deltaTicks) * 1000.0) / static_cast<double>(qpcFrequency_);

    if (deltaMs < minDeltaMs_) {
        // Sub-millisecond duplicate or nested DxgKrnl event: ignore, keep original timestamp
        return std::nullopt;
    }

    it->second = qpcTimestamp;
    return deltaMs;
}

void FrameTimeCalculator::reset(uint32_t processId) {
    lastTimestamps_.erase(processId);
}

void FrameTimeCalculator::clear() {
    lastTimestamps_.clear();
}

}  // namespace perfoverlay::capture

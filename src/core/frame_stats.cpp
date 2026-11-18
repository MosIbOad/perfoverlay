#include "core/frame_stats.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace perfoverlay::core {

FrameTimeBuffer::FrameTimeBuffer(size_t maxCapacity, Duration maxDuration)
    : capacity_(std::max<size_t>(1, maxCapacity)),
      maxDuration_(maxDuration) {
    buffer_.resize(capacity_);
}

void FrameTimeBuffer::push(double frameTimeMs) {
    push(frameTimeMs, std::chrono::steady_clock::now());
}

void FrameTimeBuffer::push(double frameTimeMs, TimePoint timestamp) {
    if (capacity_ == 0) {
        return;
    }

    // Prune entries that exceed our max duration window
    pruneOldEntries(timestamp);

    // If still at capacity after pruning, drop the oldest entry
    if (size_ == capacity_) {
        tail_ = (tail_ + 1) % capacity_;
        --size_;
    }

    buffer_[head_] = FrameEntry{frameTimeMs, timestamp};
    head_ = (head_ + 1) % capacity_;
    ++size_;
}

void FrameTimeBuffer::discardOlderThan(Duration maxAge, TimePoint currentTime) {
    if (maxAge <= Duration::zero() || size_ == 0) {
        return;
    }
    TimePoint cutoff = currentTime - maxAge;
    discardOlderThan(cutoff);
}

void FrameTimeBuffer::discardOlderThan(TimePoint cutoff) {
    while (size_ > 0 && buffer_[tail_].timestamp < cutoff) {
        tail_ = (tail_ + 1) % capacity_;
        --size_;
    }
}

void FrameTimeBuffer::clear() noexcept {
    head_ = 0;
    tail_ = 0;
    size_ = 0;
}

double FrameTimeBuffer::operator[](size_t index) const {
    return buffer_[(tail_ + index) % capacity_].frameTimeMs;
}

const FrameEntry& FrameTimeBuffer::entryAt(size_t index) const {
    if (index >= size_) {
        throw std::out_of_range("FrameTimeBuffer index out of range");
    }
    return buffer_[(tail_ + index) % capacity_];
}

double FrameTimeBuffer::front() const {
    if (size_ == 0) {
        return 0.0;
    }
    return buffer_[tail_].frameTimeMs;
}

double FrameTimeBuffer::back() const {
    if (size_ == 0) {
        return 0.0;
    }
    return buffer_[(tail_ + size_ - 1) % capacity_].frameTimeMs;
}

void FrameTimeBuffer::copyFrameTimes(std::span<double> out) const {
    const size_t count = std::min(size_, out.size());
    for (size_t i = 0; i < count; ++i) {
        out[i] = buffer_[(tail_ + i) % capacity_].frameTimeMs;
    }
}

void FrameTimeBuffer::copyFrameTimes(std::vector<double>& out) const {
    out.resize(size_);
    if (size_ > 0) {
        copyFrameTimes(std::span<double>(out.data(), out.size()));
    }
}

void FrameTimeBuffer::pruneOldEntries(TimePoint currentTime) {
    if (maxDuration_ <= Duration::zero() || size_ == 0) {
        return;
    }
    TimePoint cutoff = currentTime - maxDuration_;
    discardOlderThan(cutoff);
}

FrameStats computeStats(const FrameTimeBuffer& buffer, std::vector<double>& scratch) {
    if (buffer.empty()) {
        return FrameStats{};
    }

    const size_t N = buffer.size();
    scratch.resize(N);
    buffer.copyFrameTimes(std::span<double>(scratch.data(), N));

    double totalMs = 0.0;
    for (double ms : scratch) {
        totalMs += ms;
    }

    const double avgFrameTimeMs = totalMs / static_cast<double>(N);
    const double avgFps = (avgFrameTimeMs > 0.0) ? (1000.0 / avgFrameTimeMs) : 0.0;

    const double lastMs = buffer.back();
    const double currentFps = (lastMs > 0.0) ? (1000.0 / lastMs) : 0.0;

    if (N == 1) {
        return FrameStats{
            .avgFps = avgFps,
            .currentFps = currentFps,
            .minFps = currentFps,
            .maxFps = currentFps,
            .p1LowFps = currentFps,
            .p01LowFps = currentFps,
            .avgFrameTimeMs = avgFrameTimeMs,
            .p99FrameTimeMs = avgFrameTimeMs,
        };
    }

    // Sort ascending: scratch[0] = fastest (lowest ms), scratch[N-1] = slowest (highest ms)
    std::sort(scratch.begin(), scratch.end());

    const double minMs = scratch.front();
    const double maxMs = scratch.back();

    const double minFps = (maxMs > 0.0) ? (1000.0 / maxMs) : 0.0;
    const double maxFps = (minMs > 0.0) ? (1000.0 / minMs) : 0.0;

    // 99th percentile frame time (nearest rank)
    size_t p99Idx = static_cast<size_t>(std::ceil(0.99 * static_cast<double>(N)));
    if (p99Idx > 0) {
        --p99Idx;
    }
    if (p99Idx >= N) {
        p99Idx = N - 1;
    }
    const double p99FrameTimeMs = scratch[p99Idx];

    // 1% Low FPS:
    // Defined as the FPS equivalent of the average frame time of the worst 1% frames.
    // We select the slowest ceil(N * 0.01) frames from the end of the sorted array,
    // compute their arithmetic mean frame time, and take 1000.0 / mean.
    size_t k1 = static_cast<size_t>(std::ceil(static_cast<double>(N) * 0.01));
    k1 = std::clamp(k1, size_t{1}, N);

    double worst1Sum = 0.0;
    for (size_t i = N - k1; i < N; ++i) {
        worst1Sum += scratch[i];
    }
    const double worst1AvgMs = worst1Sum / static_cast<double>(k1);
    const double p1LowFps = (worst1AvgMs > 0.0) ? (1000.0 / worst1AvgMs) : 0.0;

    // 0.1% Low FPS:
    // Average frame time of the slowest ceil(N * 0.001) frames, converted to FPS.
    size_t k01 = static_cast<size_t>(std::ceil(static_cast<double>(N) * 0.001));
    k01 = std::clamp(k01, size_t{1}, N);

    double worst01Sum = 0.0;
    for (size_t i = N - k01; i < N; ++i) {
        worst01Sum += scratch[i];
    }
    const double worst01AvgMs = worst01Sum / static_cast<double>(k01);
    const double p01LowFps = (worst01AvgMs > 0.0) ? (1000.0 / worst01AvgMs) : 0.0;

    return FrameStats{
        .avgFps = avgFps,
        .currentFps = currentFps,
        .minFps = minFps,
        .maxFps = maxFps,
        .p1LowFps = p1LowFps,
        .p01LowFps = p01LowFps,
        .avgFrameTimeMs = avgFrameTimeMs,
        .p99FrameTimeMs = p99FrameTimeMs,
    };
}

FrameStats computeStats(const FrameTimeBuffer& buffer) {
    std::vector<double> scratch;
    return computeStats(buffer, scratch);
}

} // namespace perfoverlay::core

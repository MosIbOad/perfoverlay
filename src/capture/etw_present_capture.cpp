#include "capture/etw_present_capture.hpp"
#include "capture/etw_types.hpp"
#include "capture/frame_time_calculator.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace perfoverlay::capture {

namespace {

constexpr wchar_t SESSION_NAME[] = L"perfoverlay_etw_present_session";

void stopSessionByName(const wchar_t* sessionName) {
    const ULONG bufSize = sizeof(EVENT_TRACE_PROPERTIES) + 512 * sizeof(wchar_t);
    std::vector<BYTE> buf(bufSize, 0);
    auto* props = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buf.data());
    props->Wnode.BufferSize = bufSize;
    props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    ControlTraceW(0, sessionName, props, EVENT_TRACE_CONTROL_STOP);
}

// Emergency cleanup handler in case of Ctrl+C or console close
BOOL WINAPI ConsoleCleanupHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_CLOSE_EVENT || ctrlType == CTRL_SHUTDOWN_EVENT) {
        stopSessionByName(SESSION_NAME);
    }
    return FALSE;
}

} // namespace

class EtwPresentCapture::Impl {
public:
    Impl(size_t bufferCapacity, std::chrono::nanoseconds bufferDuration)
        : bufferCapacity_(bufferCapacity),
          bufferDuration_(bufferDuration),
          calculator_(1) {
        LARGE_INTEGER qpcFreq{};
        if (QueryPerformanceFrequency(&qpcFreq) && qpcFreq.QuadPart > 0) {
            calculator_ = FrameTimeCalculator(static_cast<uint64_t>(qpcFreq.QuadPart));
        }
        SetConsoleCtrlHandler(ConsoleCleanupHandler, TRUE);
    }

    ~Impl() {
        stop();
    }

    EtwStartResult start() {
        if (running_.load()) {
            return EtwStartResult{.status = EtwStartStatus::AlreadyRunning};
        }

        // Clean up any stale session with our name left behind by previous crashes
        stopSessionByName(SESSION_NAME);

        const ULONG bufSize = sizeof(EVENT_TRACE_PROPERTIES) + 512 * sizeof(wchar_t);
        std::vector<BYTE> propBuffer(bufSize, 0);
        auto* props = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(propBuffer.data());
        props->Wnode.BufferSize = bufSize;
        props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
        props->Wnode.ClientContext = 1; // 1 = QueryPerformanceCounter resolution
        props->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
        props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
        props->BufferSize = 64; // 64 KB buffers
        props->MinimumBuffers = 4;
        props->MaximumBuffers = 32;
        props->FlushTimer = 1;

        TRACEHANDLE sessionHandle = 0;
        ULONG status = StartTraceW(&sessionHandle, SESSION_NAME, props);

        if (status == ERROR_ACCESS_DENIED) {
            return EtwStartResult{
                .status = EtwStartStatus::AccessDenied,
                .errorMessage =
                    "ETW access denied (Win32 Error 5).\n"
                    "Administrator privileges or membership in the 'Performance Log Users' group is required.\n"
                    "To resolve:\n"
                    "  Option 1: Run perfoverlay as Administrator.\n"
                    "  Option 2: Add your user to the group once: net localgroup \"Performance Log Users\" \"%USERNAME%\" /add\n"
                    "            (then log off and log back in)."
            };
        }

        if (status == ERROR_ALREADY_EXISTS) {
            stopSessionByName(SESSION_NAME);
            status = StartTraceW(&sessionHandle, SESSION_NAME, props);
        }

        if (status != ERROR_SUCCESS) {
            return EtwStartResult{
                .status = EtwStartStatus::SessionStartFailed,
                .errorMessage = "StartTraceW failed with Win32 error: " + std::to_string(status)
            };
        }

        sessionHandle_ = sessionHandle;

        // Enable Microsoft-Windows-DXGI Provider
        ENABLE_TRACE_PARAMETERS enableParams{};
        enableParams.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
        enableParams.EnableProperty = EVENT_ENABLE_PROPERTY_IGNORE_KEYWORD_0;

        EnableTraceEx2(
            sessionHandle_,
            &DXGI_PROVIDER_GUID,
            EVENT_CONTROL_CODE_ENABLE_PROVIDER,
            TRACE_LEVEL_INFORMATION,
            0,
            0,
            0,
            &enableParams
        );

        // Enable Microsoft-Windows-DxgKrnl Provider
        EnableTraceEx2(
            sessionHandle_,
            &DXGKRNL_PROVIDER_GUID,
            EVENT_CONTROL_CODE_ENABLE_PROVIDER,
            TRACE_LEVEL_INFORMATION,
            0,
            0,
            0,
            &enableParams
        );

        running_.store(true);

        // Launch consumer in background thread
        worker_ = std::jthread([this](std::stop_token stopToken) {
            consumeTrace(stopToken);
        });

        return EtwStartResult{.status = EtwStartStatus::Success};
    }

    void stop() {
        if (!running_.exchange(false)) {
            return;
        }

        // Close consumer trace handle to unblock ProcessTrace
        const TRACEHANDLE trace = traceHandle_.exchange(INVALID_PROCESSTRACE_HANDLE);
        if (trace != INVALID_PROCESSTRACE_HANDLE && trace != 0) {
            CloseTrace(trace);
        }

        // Stop kernel session
        if (sessionHandle_ != 0) {
            stopSessionByName(SESSION_NAME);
            sessionHandle_ = 0;
        }

        if (worker_.joinable()) {
            worker_.request_stop();
            worker_.join();
        }
    }

    [[nodiscard]] bool isRunning() const noexcept {
        return running_.load();
    }

    [[nodiscard]] std::optional<core::FrameStats> getProcessStats(uint32_t processId) const {
        std::lock_guard lock(buffersMutex_);
        auto it = processBuffers_.find(processId);
        if (it == processBuffers_.end() || it->second.empty()) {
            return std::nullopt;
        }
        return core::computeStats(it->second);
    }

    [[nodiscard]] std::optional<core::FrameTimeBuffer> getProcessBuffer(uint32_t processId) const {
        std::lock_guard lock(buffersMutex_);
        auto it = processBuffers_.find(processId);
        if (it == processBuffers_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    [[nodiscard]] std::vector<uint32_t> getTrackedProcessIds() const {
        std::lock_guard lock(buffersMutex_);
        std::vector<uint32_t> pids;
        pids.reserve(processBuffers_.size());
        for (const auto& [pid, _] : processBuffers_) {
            pids.push_back(pid);
        }
        return pids;
    }

    void pruneInactiveProcesses(std::chrono::nanoseconds maxIdleTime) {
        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(buffersMutex_);
        for (auto it = lastSeenTime_.begin(); it != lastSeenTime_.end();) {
            if ((now - it->second) > maxIdleTime) {
                processBuffers_.erase(it->first);
                it = lastSeenTime_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    static VOID WINAPI EtwEventCallback(PEVENT_RECORD pEventRecord) {
        if (!pEventRecord || !pEventRecord->UserContext) {
            return;
        }
        auto* self = static_cast<Impl*>(pEventRecord->UserContext);
        self->handleRecord(pEventRecord);
    }

    void handleRecord(PEVENT_RECORD pEventRecord) {
        const auto& header = pEventRecord->EventHeader;
        const uint32_t pid = header.ProcessId;
        if (pid <= 4) {
            return;
        }

        const uint16_t eventId = header.EventDescriptor.Id;
        const GUID& providerId = header.ProviderId;

        if (!isPresentStartEvent(providerId, eventId)) {
            return;
        }

        const uint64_t qpcTime = static_cast<uint64_t>(header.TimeStamp.QuadPart);

        std::optional<double> frameTimeMs;
        {
            std::lock_guard lock(calcMutex_);
            frameTimeMs = calculator_.processEvent(pid, qpcTime);
        }

        if (frameTimeMs.has_value() && *frameTimeMs > 0.0) {
            std::lock_guard lock(buffersMutex_);
            auto it = processBuffers_.find(pid);
            if (it == processBuffers_.end()) {
                it = processBuffers_.emplace(
                    pid,
                    core::FrameTimeBuffer(bufferCapacity_, bufferDuration_)
                ).first;
            }
            it->second.push(*frameTimeMs);
            lastSeenTime_[pid] = std::chrono::steady_clock::now();
        }
    }

    void consumeTrace(std::stop_token /*stopToken*/) {
        EVENT_TRACE_LOGFILEW logFile{};
        logFile.LoggerName = const_cast<LPWSTR>(SESSION_NAME);
        logFile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
        logFile.EventRecordCallback = &EtwEventCallback;
        logFile.Context = this;

        const TRACEHANDLE traceHandle = OpenTraceW(&logFile);
        if (traceHandle == INVALID_PROCESSTRACE_HANDLE) {
            running_.store(false);
            return;
        }
        traceHandle_.store(traceHandle);

        TRACEHANDLE handles[1] = {traceHandle};
        ProcessTrace(handles, 1, nullptr, nullptr);

        const TRACEHANDLE prev = traceHandle_.exchange(INVALID_PROCESSTRACE_HANDLE);
        if (prev != INVALID_PROCESSTRACE_HANDLE && prev != 0) {
            CloseTrace(prev);
        }
    }

    size_t bufferCapacity_{2000};
    std::chrono::nanoseconds bufferDuration_{std::chrono::seconds(10)};

    std::atomic<bool> running_{false};
    TRACEHANDLE sessionHandle_{0};
    std::atomic<TRACEHANDLE> traceHandle_{INVALID_PROCESSTRACE_HANDLE};
    std::jthread worker_;

    mutable std::mutex calcMutex_;
    FrameTimeCalculator calculator_;

    mutable std::mutex buffersMutex_;
    std::unordered_map<uint32_t, core::FrameTimeBuffer> processBuffers_;
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> lastSeenTime_;
};

EtwPresentCapture::EtwPresentCapture(size_t bufferCapacity, std::chrono::nanoseconds bufferDuration)
    : impl_(std::make_unique<Impl>(bufferCapacity, bufferDuration)) {}

EtwPresentCapture::~EtwPresentCapture() = default;

EtwStartResult EtwPresentCapture::start() {
    return impl_->start();
}

void EtwPresentCapture::stop() {
    impl_->stop();
}

bool EtwPresentCapture::isRunning() const noexcept {
    return impl_->isRunning();
}

std::optional<core::FrameStats> EtwPresentCapture::getProcessStats(uint32_t processId) const {
    return impl_->getProcessStats(processId);
}

std::optional<core::FrameTimeBuffer> EtwPresentCapture::getProcessBuffer(uint32_t processId) const {
    return impl_->getProcessBuffer(processId);
}

std::vector<uint32_t> EtwPresentCapture::getTrackedProcessIds() const {
    return impl_->getTrackedProcessIds();
}

void EtwPresentCapture::pruneInactiveProcesses(std::chrono::nanoseconds maxIdleTime) {
    impl_->pruneInactiveProcesses(maxIdleTime);
}

} // namespace perfoverlay::capture

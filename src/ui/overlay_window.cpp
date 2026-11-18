#include "ui/overlay_window.hpp"

#include <d3d11.h>
#include <dxgi.h>
#include <dwmapi.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace perfoverlay::ui {

namespace {

constexpr wchar_t WINDOW_CLASS_NAME[] = L"perfoverlay_window_class";
constexpr int HOTKEY_ID_F10 = 0x1010;

} // namespace

OverlayWindow::OverlayWindow(const OverlayConfig& config)
    : config_(config) {}

OverlayWindow::~OverlayWindow() {
    shutdown();
}

bool OverlayWindow::init() {
    if (isRunning_) {
        return true;
    }

    if (!initWindow()) {
        return false;
    }

    if (!initD3D()) {
        shutdown();
        return false;
    }

    if (!initImGui()) {
        shutdown();
        return false;
    }

    isRunning_ = true;
    return true;
}

void OverlayWindow::shutdown() {
    isRunning_ = false;

    cleanupImGui();
    cleanupD3D();

    if (hwnd_) {
        UnregisterHotKey(hwnd_, HOTKEY_ID_F10);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    UnregisterClassW(WINDOW_CLASS_NAME, GetModuleHandle(nullptr));
}

bool OverlayWindow::initWindow() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    RegisterClassExW(&wc);

    // Click-through, transparent, always on top, no taskbar button, no focus theft
    const DWORD exStyle = WS_EX_LAYERED |
                          WS_EX_TRANSPARENT |
                          WS_EX_TOPMOST |
                          WS_EX_TOOLWINDOW |
                          WS_EX_NOACTIVATE;
    const DWORD style = WS_POPUP;

    hwnd_ = CreateWindowExW(
        exStyle,
        WINDOW_CLASS_NAME,
        L"perfoverlay",
        style,
        config_.offsetX,
        config_.offsetY,
        config_.initialWidth,
        config_.initialHeight,
        nullptr,
        nullptr,
        wc.hInstance,
        this
    );

    if (!hwnd_) {
        return false;
    }

    // Extend DWM frame into entire client area to enable true per-pixel alpha blending
    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd_, &margins);
    SetLayeredWindowAttributes(hwnd_, RGB(0, 0, 0), 255, LWA_ALPHA);

    // Register global hotkey F10
    RegisterHotKey(hwnd_, HOTKEY_ID_F10, 0, VK_F10);

    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    UpdateWindow(hwnd_);

    return true;
}

bool OverlayWindow::initD3D() {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = config_.initialWidth;
    sd.BufferDesc.Height = config_.initialHeight;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd_;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &swapChain_,
        &d3dDevice_,
        &featureLevel,
        &d3dContext_
    );

    if (FAILED(hr)) {
        // Fallback to WARP software rasterizer if hardware device is unavailable
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &swapChain_,
            &d3dDevice_,
            &featureLevel,
            &d3dContext_
        );
        if (FAILED(hr)) {
            return false;
        }
    }

    createRenderTarget();
    return true;
}

void OverlayWindow::cleanupD3D() {
    cleanupRenderTarget();
    if (swapChain_) {
        swapChain_->Release();
        swapChain_ = nullptr;
    }
    if (d3dContext_) {
        d3dContext_->Release();
        d3dContext_ = nullptr;
    }
    if (d3dDevice_) {
        d3dDevice_->Release();
        d3dDevice_ = nullptr;
    }
}

void OverlayWindow::createRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) {
        d3dDevice_->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView_);
        backBuffer->Release();
    }
}

void OverlayWindow::cleanupRenderTarget() {
    if (renderTargetView_) {
        renderTargetView_->Release();
        renderTargetView_ = nullptr;
    }
}

bool OverlayWindow::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr; // Do not save settings to disk

    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(hwnd_)) {
        return false;
    }
    if (!ImGui_ImplDX11_Init(d3dDevice_, d3dContext_)) {
        return false;
    }

    return true;
}

void OverlayWindow::cleanupImGui() {
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
}

bool OverlayWindow::pollMessages() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            isRunning_ = false;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return isRunning_;
}

void OverlayWindow::updateMonitorPosition(HWND targetHwnd) {
    if (!hwnd_) {
        return;
    }

    HMONITOR targetMonitor = nullptr;
    if (targetHwnd && targetHwnd != hwnd_ && IsWindow(targetHwnd)) {
        targetMonitor = MonitorFromWindow(targetHwnd, MONITOR_DEFAULTTONEAREST);
    }
    if (!targetMonitor) {
        targetMonitor = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    }

    if (targetMonitor != currentMonitor_) {
        currentMonitor_ = targetMonitor;
        MONITORINFO mi{};
        mi.cbSize = sizeof(MONITORINFO);
        if (GetMonitorInfoA(targetMonitor, &mi)) {
            const int newX = mi.rcMonitor.left + config_.offsetX;
            const int newY = mi.rcMonitor.top + config_.offsetY;
            SetWindowPos(
                hwnd_,
                HWND_TOPMOST,
                newX,
                newY,
                config_.initialWidth,
                config_.initialHeight,
                SWP_NOACTIVATE | SWP_NOZORDER
            );
        }
    }
}

void OverlayWindow::render(const core::MetricsSnapshot& snapshot) {
    if (!isRunning_ || !isVisible_) {
        return;
    }

    // Auto-detect monitor of the active foreground window
    const HWND activeHwnd = GetForegroundWindow();
    updateMonitorPosition(activeHwnd);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Fill overlay client area
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        ImVec2(static_cast<float>(config_.initialWidth), static_cast<float>(config_.initialHeight)),
        ImGuiCond_Always
    );

    const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
                                         ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoFocusOnAppearing |
                                         ImGuiWindowFlags_NoNav |
                                         ImGuiWindowFlags_NoMove;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.08f, 0.78f)); // Dark translucent glass
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.30f, 0.50f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));

    if (ImGui::Begin("##perfoverlay_panel", nullptr, windowFlags)) {
        // Header
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "PERFOVERLAY");
        ImGui::SameLine();
        ImGui::TextDisabled("| F10: toggle");

        if (snapshot.hasActiveGame && snapshot.activeGame.has_value()) {
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Game: %s", snapshot.activeGame->processName.c_str());
        } else {
            ImGui::TextDisabled("Game: [Waiting for game focus...]");
        }
        ImGui::Separator();

        // Frame timing & FPS
        if (snapshot.frameStats.has_value() && snapshot.frameStats->avgFps > 0.0) {
            const auto& f = *snapshot.frameStats;
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "FPS: %.1f", f.currentFps);
            ImGui::SameLine(140.0f);
            ImGui::TextDisabled("Avg: %.1f", f.avgFps);

            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "1%% Low: %.1f", f.p1LowFps);
            ImGui::SameLine(140.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f), "0.1%% Low: %.1f", f.p01LowFps);

            ImGui::Text("Frametime: %.2f ms", f.avgFrameTimeMs);
            ImGui::SameLine(140.0f);
            ImGui::TextDisabled("p99: %.2f ms", f.p99FrameTimeMs);
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "FPS: N/A");
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "1%% Low: N/A  |  0.1%% Low: N/A");
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Frametime: N/A");
        }
        ImGui::Separator();

        // Hardware Telemetry
        // CPU Usage & Temperature
        if (snapshot.sensors.cpuUsagePercent.has_value()) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "CPU: %.1f %%", *snapshot.sensors.cpuUsagePercent);
        } else {
            ImGui::TextDisabled("CPU: N/A");
        }

        ImGui::SameLine(140.0f);
        if (snapshot.sensors.cpuTempC.has_value()) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Temp: %.0f C", *snapshot.sensors.cpuTempC);
        } else {
            ImGui::TextDisabled("Temp: N/A");
        }

        // GPU Usage & Temperature
        if (snapshot.sensors.gpuUsagePercent.has_value()) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.3f, 1.0f), "GPU: %.1f %%", *snapshot.sensors.gpuUsagePercent);
        } else {
            ImGui::TextDisabled("GPU: N/A");
        }

        ImGui::SameLine(140.0f);
        if (snapshot.sensors.gpuTempC.has_value()) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Temp: %.0f C", *snapshot.sensors.gpuTempC);
        } else {
            ImGui::TextDisabled("Temp: N/A");
        }

        // VRAM
        if (snapshot.sensors.gpuVramUsedMB.has_value() && snapshot.sensors.gpuVramTotalMB.has_value()) {
            ImGui::Text("VRAM: %.0f / %.0f MB", *snapshot.sensors.gpuVramUsedMB, *snapshot.sensors.gpuVramTotalMB);
        } else if (snapshot.sensors.gpuVramUsedMB.has_value()) {
            ImGui::Text("VRAM: %.0f MB", *snapshot.sensors.gpuVramUsedMB);
        } else {
            ImGui::TextDisabled("VRAM: N/A");
        }

        // Clock & Power (if available)
        if (snapshot.sensors.gpuClockMHz.has_value()) {
            ImGui::TextDisabled("Clock: %.0f MHz", *snapshot.sensors.gpuClockMHz);
        }
        if (snapshot.sensors.gpuPowerW.has_value()) {
            if (snapshot.sensors.gpuClockMHz.has_value()) {
                ImGui::SameLine(140.0f);
            }
            ImGui::TextDisabled("Power: %.1f W", *snapshot.sensors.gpuPowerW);
        }

        // Upscaler Telemetry (Detected in game files)
        ImGui::Separator();
        if (snapshot.upscaler.has_value() && snapshot.upscaler->anyDetected()) {
            ImGui::TextColored(ImVec4(0.75f, 0.55f, 1.0f, 1.0f), "Upscaler: %s", snapshot.upscaler->formatSummary().c_str());
            ImGui::TextDisabled("Detected in game files");
        } else if (snapshot.hasActiveGame) {
            ImGui::TextDisabled("Upscaler: None detected");
        } else {
            ImGui::TextDisabled("Upscaler: N/A");
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    ImGui::Render();

    // Fully transparent clear: per-pixel alpha = 0.0f
    const float clearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    d3dContext_->OMSetRenderTargets(1, &renderTargetView_, nullptr);
    d3dContext_->ClearRenderTargetView(renderTargetView_, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    swapChain_->Present(1, 0); // VSync enabled to throttle render loop
}

void OverlayWindow::setVisible(bool visible) {
    if (isVisible_ != visible) {
        isVisible_ = visible;
        if (hwnd_) {
            ShowWindow(hwnd_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
        }
    }
}

void OverlayWindow::toggleVisibility() {
    setVisible(!isVisible_);
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return true;
    }

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    auto* self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_HOTKEY:
        if (wParam == HOTKEY_ID_F10 && self) {
            self->toggleVisibility();
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace perfoverlay::ui

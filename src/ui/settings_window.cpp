#include "ui/settings_window.hpp"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace perfoverlay::ui {

namespace {
constexpr int SETTINGS_WIDTH = 480;
constexpr int SETTINGS_HEIGHT = 580;
}  // namespace

SettingsWindow::SettingsWindow() = default;

SettingsWindow::~SettingsWindow() {
    if (imguiContext_) {
        ImGui::SetCurrentContext(imguiContext_);
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(imguiContext_);
        imguiContext_ = nullptr;
    }
    cleanupD3D();
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

LRESULT CALLBACK SettingsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* window = reinterpret_cast<SettingsWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (window && window->imguiContext_) {
        ImGui::SetCurrentContext(window->imguiContext_);
    }

    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return true;
    }

    switch (msg) {
        case WM_CLOSE:
            if (window) {
                window->hide();
            }
            return 0;

        case WM_DESTROY:
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

bool SettingsWindow::init(HWND /*parentHwnd*/) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"perfoverlay_settings_window";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"perfoverlay - Ayarlar (Settings)",
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
                            SETTINGS_WIDTH, SETTINGS_HEIGHT, nullptr, nullptr, wc.hInstance, nullptr);

    if (!hwnd_) {
        return false;
    }

    SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    if (!initD3D()) {
        return false;
    }

    // Initialize dedicated Dear ImGui context for settings window
    IMGUI_CHECKVERSION();
    imguiContext_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imguiContext_);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(d3dDevice_, d3dContext_);

    return true;
}

bool SettingsWindow::initD3D() {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = SETTINGS_WIDTH;
    sd.BufferDesc.Height = SETTINGS_HEIGHT;
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

    HRESULT hr =
        D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray,
                                      2, D3D11_SDK_VERSION, &sd, &swapChain_, &d3dDevice_, &featureLevel, &d3dContext_);

    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray,
                                           2, D3D11_SDK_VERSION, &sd, &swapChain_, &d3dDevice_, &featureLevel,
                                           &d3dContext_);
        if (FAILED(hr)) {
            return false;
        }
    }

    ID3D11Texture2D* backBuffer{nullptr};
    hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) {
        return false;
    }

    hr = d3dDevice_->CreateRenderTargetView(backBuffer, nullptr, &renderTargetView_);
    backBuffer->Release();

    return SUCCEEDED(hr);
}

void SettingsWindow::cleanupD3D() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();

    if (renderTargetView_) {
        renderTargetView_->Release();
        renderTargetView_ = nullptr;
    }
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

void SettingsWindow::show(const core::AppConfig& currentConfig) {
    workingConfig_ = currentConfig;
    isVisible_ = true;
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
}

void SettingsWindow::hide() {
    isVisible_ = false;
    ShowWindow(hwnd_, SW_HIDE);
}

bool SettingsWindow::render() {
    if (!isVisible_) {
        return true;
    }

    MSG msg;
    while (PeekMessage(&msg, hwnd_, 0U, 0U, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT) {
            return false;
        }
    }

    if (!isVisible_) {
        return true;
    }

    ImGui::SetCurrentContext(imguiContext_);

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RECT clientRect;
    GetClientRect(hwnd_, &clientRect);
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(clientRect.right), static_cast<float>(clientRect.bottom)),
                             ImGuiCond_Always);

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    if (ImGui::Begin("SettingsPanel", nullptr, flags)) {
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "PERFOVERLAY AYARLARI");
        ImGui::Separator();

        // 1. Metric Visibility
        if (ImGui::CollapsingHeader("Gorunecek Metrikler (Metrics)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("FPS (Anlik & Ortalama)", &workingConfig_.metrics.showFps);
            ImGui::SameLine(240);
            ImGui::Checkbox("Frame Time (ms)", &workingConfig_.metrics.showFrameTime);

            ImGui::Checkbox("1% Low FPS", &workingConfig_.metrics.show1PercentLow);
            ImGui::SameLine(240);
            ImGui::Checkbox("0.1% Low FPS", &workingConfig_.metrics.show01PercentLow);

            ImGui::Separator();
            ImGui::Checkbox("CPU Kullanimi (%)", &workingConfig_.metrics.showCpuUsage);
            ImGui::SameLine(240);
            ImGui::Checkbox("CPU Sicakligi (C)", &workingConfig_.metrics.showCpuTemp);

            ImGui::Checkbox("GPU Kullanimi (%)", &workingConfig_.metrics.showGpuUsage);
            ImGui::SameLine(240);
            ImGui::Checkbox("GPU Sicakligi (C)", &workingConfig_.metrics.showGpuTemp);

            ImGui::Checkbox("VRAM Kullanimi (MB)", &workingConfig_.metrics.showVram);
            ImGui::SameLine(240);
            ImGui::Checkbox("GPU Saat Hizi (MHz)", &workingConfig_.metrics.showGpuClock);

            ImGui::Checkbox("GPU Guc Tuketimi (W)", &workingConfig_.metrics.showGpuPower);
            ImGui::SameLine(240);
            ImGui::Checkbox("Upscaler (DLSS/FSR/XeSS)", &workingConfig_.metrics.showUpscaler);
        }

        // 2. Overlay Placement
        if (ImGui::CollapsingHeader("Konum ve Boyut (Placement & Sizing)", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* corners[] = {"Sol Ust (Top-Left)", "Sag Ust (Top-Right)", "Sol Alt (Bottom-Left)",
                                     "Sag Alt (Bottom-Right)"};
            int currentCorner = static_cast<int>(workingConfig_.corner);
            if (ImGui::Combo("Ekran Kosesi", &currentCorner, corners, IM_ARRAYSIZE(corners))) {
                workingConfig_.corner = static_cast<core::OverlayCorner>(currentCorner);
            }

            ImGui::SliderInt("Offset X (px)", &workingConfig_.offsetX, 0, 300);
            ImGui::SliderInt("Offset Y (px)", &workingConfig_.offsetY, 0, 300);
            ImGui::SliderInt("Genislik (px)", &workingConfig_.windowWidth, 260, 500);
            ImGui::SliderInt("Yukseklik (px)", &workingConfig_.windowHeight, 180, 450);
        }

        // 3. Aesthetics & Hardware Providers
        if (ImGui::CollapsingHeader("Gorunum ve Donanim (Aesthetics & Hardware)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Arka Plan Saydamligi", &workingConfig_.backgroundAlpha, 0.1f, 1.0f, "%.2f");

            int fpsPoll = static_cast<int>(workingConfig_.fpsPollIntervalMs);
            if (ImGui::SliderInt("FPS Yenileme Araligi (ms)", &fpsPoll, 20, 500, "%d ms")) {
                workingConfig_.fpsPollIntervalMs = static_cast<uint32_t>(fpsPoll);
            }

            const char* providers[] = {"Otomatik (Auto)", "LibreHardwareMonitor (HTTP)", "Devre Disi (Disabled)"};
            int providerIndex = 0;
            if (workingConfig_.cpuTempProvider == "librehardwaremonitor") {
                providerIndex = 1;
            } else if (workingConfig_.cpuTempProvider == "disabled") {
                providerIndex = 2;
            }

            if (ImGui::Combo("CPU Sicaklik Saglayicisi", &providerIndex, providers, IM_ARRAYSIZE(providers))) {
                if (providerIndex == 0)
                    workingConfig_.cpuTempProvider = "auto";
                else if (providerIndex == 1)
                    workingConfig_.cpuTempProvider = "librehardwaremonitor";
                else
                    workingConfig_.cpuTempProvider = "disabled";
            }

            ImGui::TextDisabled("Kisayol tusu: %s (Varsayilan F10)", workingConfig_.toggleHotkey.c_str());
        }

        ImGui::Separator();
        ImGui::Spacing();

        // Buttons
        if (ImGui::Button("Kaydet & Uygula", ImVec2(130, 32))) {
            core::ConfigManager::saveToFile(core::ConfigManager::getDefaultConfigPath(), workingConfig_);
            if (onConfigApplied) {
                onConfigApplied(workingConfig_);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Varsayilana Sifirla", ImVec2(140, 32))) {
            workingConfig_ = core::AppConfig{};
        }

        ImGui::SameLine();
        if (ImGui::Button("Kapat", ImVec2(90, 32))) {
            hide();
        }
    }
    ImGui::End();

    ImGui::Render();

    const float clearColor[4] = {0.12f, 0.12f, 0.14f, 1.0f};
    d3dContext_->OMSetRenderTargets(1, &renderTargetView_, nullptr);
    d3dContext_->ClearRenderTargetView(renderTargetView_, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    swapChain_->Present(1, 0);

    return true;
}

}  // namespace perfoverlay::ui

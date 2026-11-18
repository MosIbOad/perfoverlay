#pragma once

#include "core/config.hpp"
#include "core/metrics_hub.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <functional>
#include <windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;

namespace perfoverlay::ui {

using OverlayConfig = core::AppConfig;

/**
 * @brief Transparent, click-through, always-on-top external overlay window.
 *
 * Implements:
 * - Win32 styles: WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE.
 * - Borderless WS_POPUP with DWM frame extension for per-pixel alpha transparency.
 * - DirectX 11 + Dear ImGui rendering.
 * - Automatic tracking of the monitor hosting the active game window.
 * - Configurable corner positioning (TopLeft, TopRight, BottomLeft, BottomRight).
 * - Global F10 hotkey to toggle overlay visibility.
 */
class OverlayWindow {
public:
    explicit OverlayWindow(const core::AppConfig& config = {});
    ~OverlayWindow();

    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;
    OverlayWindow(OverlayWindow&&) = delete;
    OverlayWindow& operator=(OverlayWindow&&) = delete;

    /**
     * @brief Initializes the Win32 window, D3D11 device/swapchain, and ImGui.
     * @return true on success, false otherwise.
     */
    bool init();

    /**
     * @brief Cleans up ImGui, D3D11, and Win32 window resources.
     */
    void shutdown();

    /**
     * @brief Dispatches queued Windows messages (including F10 hotkey).
     * @return false if WM_QUIT was received, true to continue running.
     */
    bool pollMessages();

    /**
     * @brief Updates active configuration and re-applies window styling/position.
     */
    void setConfig(const core::AppConfig& config);
    [[nodiscard]] const core::AppConfig& getConfig() const noexcept { return config_; }

    /**
     * @brief Renders one frame of the HUD overlay with the provided metrics snapshot.
     */
    void render(const core::MetricsSnapshot& snapshot);

    /**
     * @brief Repositions the overlay to the monitor containing the target window.
     */
    void updateMonitorPosition(HWND targetHwnd);

    [[nodiscard]] bool isRunning() const noexcept { return isRunning_; }
    [[nodiscard]] bool isVisible() const noexcept { return isVisible_; }
    void setVisible(bool visible);
    void toggleVisibility();

    [[nodiscard]] HWND getHwnd() const noexcept { return hwnd_; }

    void setMessageHook(std::function<bool(HWND, UINT, WPARAM, LPARAM)> hook) { messageHook_ = std::move(hook); }

private:
    bool initWindow();
    bool initD3D();
    void cleanupD3D();
    bool initImGui();
    void cleanupImGui();

    void createRenderTarget();
    void cleanupRenderTarget();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    OverlayConfig config_;
    HWND hwnd_{nullptr};
    bool isRunning_{false};
    bool isVisible_{true};
    HMONITOR currentMonitor_{nullptr};

    ID3D11Device* d3dDevice_{nullptr};
    ID3D11DeviceContext* d3dContext_{nullptr};
    IDXGISwapChain* swapChain_{nullptr};
    ID3D11RenderTargetView* renderTargetView_{nullptr};
    std::function<bool(HWND, UINT, WPARAM, LPARAM)> messageHook_;
};

}  // namespace perfoverlay::ui

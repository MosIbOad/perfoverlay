#pragma once

#include "core/config.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <functional>

namespace perfoverlay::ui {

/**
 * @brief Interactive settings dialog rendered with ImGui in a dedicated Win32 window.
 *
 * Runs as a standard (non-click-through) window so users can comfortably interact
 * with checkboxes, sliders, and buttons to customize overlay behavior.
 */
class SettingsWindow {
public:
    SettingsWindow();
    ~SettingsWindow();

    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    /**
     * @brief Initializes the Win32 window, D3D11 device, and ImGui context.
     */
    bool init(HWND parentHwnd = nullptr);

    /**
     * @brief Shows or brings the settings window to the foreground with the given config.
     */
    void show(const core::AppConfig& currentConfig);

    /**
     * @brief Hides the settings window.
     */
    void hide();

    /**
     * @brief Polls messages and renders one frame if visible.
     * @return true while window is alive.
     */
    bool render();

    [[nodiscard]] bool isVisible() const noexcept { return isVisible_; }

    /**
     * @brief Callback invoked whenever the user clicks "Save & Apply".
     */
    std::function<void(const core::AppConfig&)> onConfigApplied;

private:
    bool initD3D();
    void cleanupD3D();

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_{nullptr};
    bool isVisible_{false};
    core::AppConfig workingConfig_;

    // Direct3D 11 resources
    ID3D11Device* d3dDevice_{nullptr};
    ID3D11DeviceContext* d3dContext_{nullptr};
    IDXGISwapChain* swapChain_{nullptr};
    ID3D11RenderTargetView* renderTargetView_{nullptr};
};

} // namespace perfoverlay::ui

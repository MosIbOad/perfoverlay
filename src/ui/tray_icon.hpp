#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

#include <cstdint>
#include <functional>
#include <string>

namespace perfoverlay::ui {

constexpr UINT WM_TRAY_CALLBACK = WM_APP + 101;
constexpr UINT ID_TRAY_TOGGLE = 2001;
constexpr UINT ID_TRAY_SETTINGS = 2002;
constexpr UINT ID_TRAY_OPEN_CONFIG = 2003;
constexpr UINT ID_TRAY_EXIT = 2004;

/**
 * @brief Manages the Windows system tray notification icon and context menu.
 */
class TrayIcon {
public:
    TrayIcon();
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    /**
     * @brief Adds the icon to the system tray.
     * @param hwnd Parent window handle receiving tray callback messages.
     * @param tooltip Text displayed when hovering over the tray icon.
     */
    bool init(HWND hwnd, const std::wstring& tooltip = L"perfoverlay");

    /**
     * @brief Removes the icon from the system notification area.
     */
    void remove();

    /**
     * @brief Displays the right-click popup context menu.
     */
    void showContextMenu(HWND hwnd);

    // Callback event hooks
    std::function<void()> onToggleVisibility;
    std::function<void()> onOpenSettings;
    std::function<void()> onOpenConfigFile;
    std::function<void()> onExit;

    /**
     * @brief Handles window messages pertaining to the tray icon.
     * @return true if message was processed.
     */
    bool handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    NOTIFYICONDATAW nid_{};
    bool added_{false};
};

}  // namespace perfoverlay::ui

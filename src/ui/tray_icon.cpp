#include "ui/tray_icon.hpp"

namespace perfoverlay::ui {

TrayIcon::TrayIcon() = default;

TrayIcon::~TrayIcon() {
    remove();
}

bool TrayIcon::init(HWND hwnd, const std::wstring& tooltip) {
    if (added_) {
        return true;
    }

    nid_.cbSize = sizeof(NOTIFYICONDATAW);
    nid_.hWnd = hwnd;
    nid_.uID = 1001;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAY_CALLBACK;
    nid_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    wcsncpy_s(nid_.szTip, tooltip.c_str(), sizeof(nid_.szTip) / sizeof(wchar_t) - 1);

    if (Shell_NotifyIconW(NIM_ADD, &nid_)) {
        added_ = true;
        return true;
    }
    return false;
}

void TrayIcon::remove() {
    if (added_) {
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        added_ = false;
    }
}

void TrayIcon::showContextMenu(HWND hwnd) {
    POINT pt{};
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) {
        return;
    }

    AppendMenuW(hMenu, MF_STRING, ID_TRAY_TOGGLE, L"G\u00f6ster / Gizle");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"Ayarlar...");
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_OPEN_CONFIG, L"Ayarlar dosyas\u0131n\u0131 a\u00e7");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"\u00c7\u0131k\u0131\u015f");

    SetForegroundWindow(hwnd);

    const int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);

    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(hMenu);

    if (cmd == ID_TRAY_TOGGLE && onToggleVisibility) {
        onToggleVisibility();
    } else if (cmd == ID_TRAY_SETTINGS && onOpenSettings) {
        onOpenSettings();
    } else if (cmd == ID_TRAY_OPEN_CONFIG && onOpenConfigFile) {
        onOpenConfigFile();
    } else if (cmd == ID_TRAY_EXIT && onExit) {
        onExit();
    }
}

bool TrayIcon::handleMessage(HWND hwnd, UINT msg, WPARAM /*wParam*/, LPARAM lParam) {
    if (msg == WM_TRAY_CALLBACK) {
        const UINT event = LOWORD(lParam);
        if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
            showContextMenu(hwnd);
            return true;
        }
        if (event == WM_LBUTTONDBLCLK) {
            if (onOpenSettings) {
                onOpenSettings();
            } else if (onToggleVisibility) {
                onToggleVisibility();
            }
            return true;
        }
    }
    return false;
}

}  // namespace perfoverlay::ui

#pragma once

#include <cstdint>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <guiddef.h>

namespace perfoverlay::capture {

// Microsoft-Windows-DXGI Provider: {CA11C036-0102-4A2D-B6E5-775A46973D26}
inline constexpr GUID DXGI_PROVIDER_GUID = {
    0xca11c036, 0x0102, 0x4a2d, {0xb6, 0xe5, 0x77, 0x5a, 0x46, 0x97, 0x3d, 0x26}
};

// Microsoft-Windows-DxgKrnl Provider: {80E190D0-FFB8-4AB9-BDBA-2AE51E048E59}
inline constexpr GUID DXGKRNL_PROVIDER_GUID = {
    0x80e190d0, 0xffb8, 0x4ab9, {0xbd, 0xba, 0x2a, 0xe5, 0x1e, 0x04, 0x8e, 0x59}
};

// PresentMon-relevant Event IDs
inline constexpr uint16_t EVENT_ID_DXGI_PRESENT_START = 42;
inline constexpr uint16_t EVENT_ID_DXGI_PRESENT_STOP  = 43;

inline constexpr uint16_t EVENT_ID_DXGKRNL_PRESENT_START        = 184;
inline constexpr uint16_t EVENT_ID_DXGKRNL_PRESENT_STOP         = 185;
inline constexpr uint16_t EVENT_ID_DXGKRNL_PRESENT_HISTORY_START = 171;

/**
 * @brief Checks if a given event represents a graphics present submission.
 *
 * Pure function: unit testable without ETW runtime.
 */
inline bool isPresentStartEvent(const GUID& providerId, uint16_t eventId) noexcept {
    if (InlineIsEqualGUID(providerId, DXGI_PROVIDER_GUID)) {
        return eventId == EVENT_ID_DXGI_PRESENT_START;
    }
    if (InlineIsEqualGUID(providerId, DXGKRNL_PROVIDER_GUID)) {
        return eventId == EVENT_ID_DXGKRNL_PRESENT_START ||
               eventId == EVENT_ID_DXGKRNL_PRESENT_HISTORY_START;
    }
    return false;
}

/**
 * @brief Checks if a given event is any present-related event (Start or Stop).
 */
inline bool isDxgkOrDxgiPresentEvent(const GUID& providerId, uint16_t eventId) noexcept {
    if (InlineIsEqualGUID(providerId, DXGI_PROVIDER_GUID)) {
        return eventId == EVENT_ID_DXGI_PRESENT_START || eventId == EVENT_ID_DXGI_PRESENT_STOP;
    }
    if (InlineIsEqualGUID(providerId, DXGKRNL_PROVIDER_GUID)) {
        return eventId == EVENT_ID_DXGKRNL_PRESENT_START ||
               eventId == EVENT_ID_DXGKRNL_PRESENT_STOP ||
               eventId == EVENT_ID_DXGKRNL_PRESENT_HISTORY_START;
    }
    return false;
}

} // namespace perfoverlay::capture

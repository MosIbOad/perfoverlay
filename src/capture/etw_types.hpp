#pragma once

#include <cstdint>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <guiddef.h>
#include <windows.h>

namespace perfoverlay::capture {

// Microsoft-Windows-DXGI Provider: {CA11C036-0102-4A2D-A6AD-F03CFED5D3C9}
inline constexpr GUID DXGI_PROVIDER_GUID = {
    0xca11c036, 0x0102, 0x4a2d, {0xa6, 0xad, 0xf0, 0x3c, 0xfe, 0xd5, 0xd3, 0xc9}};

// Microsoft-Windows-DxgKrnl Provider: {802EC45A-1E99-4B83-9920-87C98277BA9D}
inline constexpr GUID DXGKRNL_PROVIDER_GUID = {
    0x802ec45a, 0x1e99, 0x4b83, {0x99, 0x20, 0x87, 0xc9, 0x82, 0x77, 0xba, 0x9d}};

// PresentMon-relevant Event IDs
inline constexpr uint16_t EVENT_ID_DXGI_PRESENT_START = 42;
inline constexpr uint16_t EVENT_ID_DXGI_PRESENT_STOP = 43;
inline constexpr uint16_t EVENT_ID_DXGI_PRESENT_MPO_START = 55;
inline constexpr uint16_t EVENT_ID_DXGI_PRESENT_MPO_STOP = 56;

inline constexpr uint16_t EVENT_ID_DXGKRNL_PRESENT_START = 184;
inline constexpr uint16_t EVENT_ID_DXGKRNL_PRESENT_STOP = 185;

/**
 * @brief Checks if a given event represents a graphics present submission.
 *
 * For DXGI: Event 42 (Present_Start) or 55 (PresentMultiplaneOverlay_Start).
 * For DxgKrnl (Vulkan/D3DKMT): Event 184 (Dxgk_Present_Start).
 * Secondary token events (171, 259, 168) fire later during the same frame
 * and must not be treated as separate present starts to avoid multi-thousand FPS spikes.
 */
inline bool isPresentStartEvent(const GUID& providerId, uint16_t eventId) noexcept {
    if (InlineIsEqualGUID(providerId, DXGI_PROVIDER_GUID)) {
        return eventId == EVENT_ID_DXGI_PRESENT_START || eventId == EVENT_ID_DXGI_PRESENT_MPO_START;
    }
    if (InlineIsEqualGUID(providerId, DXGKRNL_PROVIDER_GUID)) {
        return eventId == EVENT_ID_DXGKRNL_PRESENT_START;
    }
    return false;
}

/**
 * @brief Checks if a given event is any present-related event (Start or Stop).
 */
inline bool isDxgkOrDxgiPresentEvent(const GUID& providerId, uint16_t eventId) noexcept {
    if (InlineIsEqualGUID(providerId, DXGI_PROVIDER_GUID)) {
        return eventId == EVENT_ID_DXGI_PRESENT_START || eventId == EVENT_ID_DXGI_PRESENT_STOP ||
               eventId == EVENT_ID_DXGI_PRESENT_MPO_START || eventId == EVENT_ID_DXGI_PRESENT_MPO_STOP;
    }
    if (InlineIsEqualGUID(providerId, DXGKRNL_PROVIDER_GUID)) {
        return eventId == EVENT_ID_DXGKRNL_PRESENT_START || eventId == EVENT_ID_DXGKRNL_PRESENT_STOP;
    }
    return false;
}

}  // namespace perfoverlay::capture

#pragma once

#include <cstdint>
#include <string>

namespace perfoverlay::upscaler {

/**
 * @brief Safely retrieves the full executable path of a process given its PID.
 *
 * ANTI-CHEAT SAFETY GUARANTEE:
 * - Uses ONLY PROCESS_QUERY_LIMITED_INFORMATION.
 * - NEVER requests PROCESS_VM_READ, PROCESS_VM_OPERATION, or PROCESS_ALL_ACCESS.
 * - Does not read process memory or query loaded modules.
 *
 * @param processId Target Windows process ID.
 * @return Full path to executable, or empty string on failure.
 */
[[nodiscard]] std::string getProcessExecutablePath(uint32_t processId) noexcept;

} // namespace perfoverlay::upscaler

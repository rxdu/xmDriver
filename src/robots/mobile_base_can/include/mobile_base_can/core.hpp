/*
 * core.hpp — Core Mobile-Base Profile CAN mapping: frame ids, extension ranges,
 * and scale factors (spec §3, §5). This is the CAN-specific half of the Core
 * Profile; the transport-neutral vocabulary (enums, frame structs, capability /
 * fault bits) lives in `mobile_base/types.hpp`. Adding a model profile requires
 * no change here — a platform picks an id from the reserved extension ranges and
 * the core never switches on it.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */

#ifndef XMOTION_MOBILE_BASE_CORE_HPP
#define XMOTION_MOBILE_BASE_CORE_HPP

#include <cstdint>

namespace xmotion::mobile_base {

// ---- CanFrame id map (spec §3). Lower id = higher bus priority. ---------------
// Core commands
inline constexpr std::uint32_t kCmdTwist = 0x100;
inline constexpr std::uint32_t kCmdMode = 0x101;
inline constexpr std::uint32_t kCmdEStop = 0x102;
inline constexpr std::uint32_t kCmdHeartbeat = 0x103;
// Core state
inline constexpr std::uint32_t kStateStatus = 0x200;
inline constexpr std::uint32_t kStateOdomTwist = 0x201;
inline constexpr std::uint32_t kStateOdomPose = 0x202;
inline constexpr std::uint32_t kStateCapabilities = 0x203;
inline constexpr std::uint32_t kStateLimits = 0x204;
inline constexpr std::uint32_t kStateBattery = 0x205;
inline constexpr std::uint32_t kStateFaults = 0x206;

// Reserved ranges for model-profile extension frames (defined per platform).
inline constexpr std::uint32_t kCmdExtBegin = 0x120, kCmdExtEnd = 0x180;    // [b,e)
inline constexpr std::uint32_t kStateExtBegin = 0x220, kStateExtEnd = 0x280;

inline constexpr bool IsCommandId(std::uint32_t id) {
  return id >= 0x100 && id <= 0x1FF;
}
inline constexpr bool IsStateId(std::uint32_t id) {
  return id >= 0x200 && id <= 0x2FF;
}
inline constexpr bool IsModelCmdId(std::uint32_t id) {
  return id >= kCmdExtBegin && id < kCmdExtEnd;
}
inline constexpr bool IsModelStateId(std::uint32_t id) {
  return id >= kStateExtBegin && id < kStateExtEnd;
}

// ---- Scale factors (spec §5) — SI unit multiplied by this yields the LSB. --
inline constexpr double kVelScale = 1000.0;     // m/s   -> mm/s     (i16)
inline constexpr double kAngVelScale = 1000.0;  // rad/s -> mrad/s   (i16)
inline constexpr double kAngleScale = 1000.0;   // rad   -> mrad     (i16)
inline constexpr double kPoseScale = 10000.0;   // m     -> 0.1 mm   (i16)
inline constexpr double kThetaScale = 10000.0;  // rad   -> 0.1 mrad (i16)
inline constexpr double kVoltScale = 100.0;     // V     -> 10 mV    (u16)
inline constexpr double kAmpScale = 100.0;      // A     -> 10 mA    (i16)

}  // namespace xmotion::mobile_base

#endif  // XMOTION_MOBILE_BASE_CORE_HPP

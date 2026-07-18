/*
 * core.hpp — Core Mobile-Base Profile constants: ids, ranges, scales, enums,
 * flag/fault bits (spec §3, §5, §7, §10). This is the STABLE framework — it
 * knows nothing about any model profile. Adding a model profile requires no
 * change here: a platform picks an id from the reserved extension ranges below
 * and a profile id for STATE_CAPABILITIES; the core never switches on it.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */

#ifndef XMOTION_MOBILE_BASE_CORE_HPP
#define XMOTION_MOBILE_BASE_CORE_HPP

#include <cstdint>

namespace xmotion::mobile_base {

// Protocol version byte (on the wire). Equals the spec major version.
inline constexpr std::uint8_t kProtocolVersion = 1;

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

// ---- Enumerations ----------------------------------------------------------
enum class ModeRequest : std::uint8_t { kStandby = 0, kAuto = 1 };
enum class ReportedMode : std::uint8_t {
  kStandby = 0, kManual = 1, kAuto = 2, kEStop = 3
};
enum class EStopAction : std::uint8_t { kEngage = 1, kClear = 2 };
enum class BaseType : std::uint8_t {
  kDiff = 0, kAckermann = 1, kOmni = 2, kMecanum = 3, kSwerve = 4, kCustom = 255
};

inline constexpr std::uint16_t kEStopClearKey = 0xC1EA;

// ---- STATE_STATUS flags (spec §7.1) ---------------------------------------
inline constexpr std::uint8_t kFlagEStopLatched = 1u << 0;
inline constexpr std::uint8_t kFlagCanFresh = 1u << 1;
inline constexpr std::uint8_t kFlagRcLinkOk = 1u << 2;
inline constexpr std::uint8_t kFlagFailsafeActive = 1u << 3;
inline constexpr std::uint8_t kFlagBusDegraded = 1u << 4;

// ---- STATE_CAPABILITIES dof_mask bits (spec §7.4) -------------------------
inline constexpr std::uint8_t kDofVx = 1u << 0;
inline constexpr std::uint8_t kDofVy = 1u << 1;
inline constexpr std::uint8_t kDofWz = 1u << 2;

// ---- Fault word bits (spec §10) -------------------------------------------
inline constexpr std::uint32_t kFaultCanTimeout = 1u << 0;
inline constexpr std::uint32_t kFaultProtoVersion = 1u << 1;
inline constexpr std::uint32_t kFaultMalformedFrame = 1u << 2;
inline constexpr std::uint32_t kFaultRcLinkLost = 1u << 3;
inline constexpr std::uint32_t kFaultActuatorCommand = 1u << 4;
inline constexpr std::uint32_t kFaultActuatorRead = 1u << 5;
inline constexpr std::uint32_t kFaultEStop = 1u << 6;
inline constexpr std::uint32_t kFaultCanBusoff = 1u << 7;
inline constexpr std::uint32_t kFaultCmdIntegrity = 1u << 8;

}  // namespace xmotion::mobile_base

#endif  // XMOTION_MOBILE_BASE_CORE_HPP

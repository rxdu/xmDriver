/*
 * types.hpp — Mobile-Base vocabulary: the SI command / state structs, enums, and
 * capability / fault bits that make up the abstract contract.
 *
 * Transport-neutral and dependency-free (C++ stdlib only): no CAN ids, scales, or
 * framing — those live in the mobile_base_can realization. Both the abstract
 * MobileBase interface and every implementation (CAN client, host-composed base)
 * speak these types, so this header is the shared vocabulary that keeps them all
 * on the same page.
 *
 * The `counter` on each frame struct is the E2E alive counter (spec §4); it is
 * meaningful only to a wire realization. A host-composed base leaves it 0.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */

#ifndef XMOTION_MOBILE_BASE_TYPES_HPP
#define XMOTION_MOBILE_BASE_TYPES_HPP

#include <cstdint>

namespace xmotion::mobile_base {

// Protocol/contract version. Equals the spec major version.
inline constexpr std::uint8_t kProtocolVersion = 1;

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

// ---- command frame types (commander -> base) -------------------------------
struct TwistCommand { double vx = 0, vy = 0, wz = 0; std::uint8_t counter = 0; };
struct ModeCommand { ModeRequest mode = ModeRequest::kStandby; std::uint8_t counter = 0; };
struct EStopCommand { EStopAction action = EStopAction::kEngage; std::uint16_t key = 0; std::uint8_t counter = 0; };
struct HeartbeatCommand { std::uint8_t version = kProtocolVersion; std::uint8_t counter = 0; };

// ---- state frame types (base -> commander) ---------------------------------
struct StatusState {
  std::uint8_t version = kProtocolVersion;
  ReportedMode mode = ReportedMode::kStandby;
  std::uint8_t flags = 0;
  std::uint16_t faults = 0;
  std::uint8_t counter = 0;
};
struct OdomTwistState { double vx = 0, vy = 0, wz = 0; std::uint8_t counter = 0; };
struct OdomPoseState { double dx = 0, dy = 0, dtheta = 0; std::uint8_t counter = 0; };
struct CapabilitiesState {
  std::uint8_t version = kProtocolVersion;
  BaseType base_type = BaseType::kCustom;
  std::uint8_t dof_mask = 0;
  std::uint8_t wheel_count = 0;
  std::uint8_t model_profile_id = 0;
  std::uint8_t model_profile_ver = 0;
  std::uint8_t counter = 0;
};
struct LimitsState { double max_vx = 0, max_vy = 0, max_wz = 0; std::uint8_t counter = 0; };
struct BatteryState { double voltage = 0, current = 0; std::uint8_t soc = 0xFF; std::uint8_t counter = 0; };
struct FaultState { std::uint32_t faults = 0; std::uint8_t last_fault = 0xFF; std::uint8_t counter = 0; };

}  // namespace xmotion::mobile_base

#endif  // XMOTION_MOBILE_BASE_TYPES_HPP

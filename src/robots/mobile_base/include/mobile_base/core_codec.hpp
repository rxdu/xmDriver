/*
 * core_codec.hpp — Core Mobile-Base Profile frame types and codec (spec §6-§7).
 *
 * Every struct carries a `counter` the caller sets (a per-id rolling counter);
 * Encode() packs the payload and stamps the E2E trailer, Decode*() enforces
 * shape + E2E CRC before unpacking. Counter *ordering* is validated by the
 * consumer via CounterCheck (see command_tracker.hpp) — decode only guarantees
 * integrity of the bytes.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */

#ifndef XMOTION_MOBILE_BASE_CORE_CODEC_HPP
#define XMOTION_MOBILE_BASE_CORE_CODEC_HPP

#include <cstdint>
#include <optional>

#include "mobile_base/codec_util.hpp"
#include "mobile_base/core.hpp"

namespace xmotion::mobile_base {

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

// ---- encode ----------------------------------------------------------------
inline CanFrame Encode(const TwistCommand& c) {
  CanFrame f = MakeFrame(kCmdTwist);
  PutI16(f, 0, SatI16(c.vx * kVelScale));
  PutI16(f, 2, SatI16(c.vy * kVelScale));
  PutI16(f, 4, SatI16(c.wz * kAngVelScale));
  StampE2e(f, c.counter);
  return f;
}
inline CanFrame Encode(const ModeCommand& c) {
  CanFrame f = MakeFrame(kCmdMode);
  f.data[0] = static_cast<std::uint8_t>(c.mode);
  StampE2e(f, c.counter);
  return f;
}
inline CanFrame Encode(const EStopCommand& c) {
  CanFrame f = MakeFrame(kCmdEStop);
  f.data[0] = static_cast<std::uint8_t>(c.action);
  PutU16(f, 1, c.key);
  StampE2e(f, c.counter);
  return f;
}
inline CanFrame Encode(const HeartbeatCommand& c) {
  CanFrame f = MakeFrame(kCmdHeartbeat);
  f.data[0] = c.version;
  StampE2e(f, c.counter);
  return f;
}
inline CanFrame Encode(const StatusState& s) {
  CanFrame f = MakeFrame(kStateStatus);
  f.data[0] = s.version;
  f.data[1] = static_cast<std::uint8_t>(s.mode);
  f.data[2] = s.flags;
  PutU16(f, 3, s.faults);
  StampE2e(f, s.counter);
  return f;
}
inline CanFrame Encode(const OdomTwistState& s) {
  CanFrame f = MakeFrame(kStateOdomTwist);
  PutI16(f, 0, SatI16(s.vx * kVelScale));
  PutI16(f, 2, SatI16(s.vy * kVelScale));
  PutI16(f, 4, SatI16(s.wz * kAngVelScale));
  StampE2e(f, s.counter);
  return f;
}
inline CanFrame Encode(const OdomPoseState& s) {
  CanFrame f = MakeFrame(kStateOdomPose);
  PutI16(f, 0, SatI16(s.dx * kPoseScale));
  PutI16(f, 2, SatI16(s.dy * kPoseScale));
  PutI16(f, 4, SatI16(s.dtheta * kThetaScale));
  StampE2e(f, s.counter);
  return f;
}
inline CanFrame Encode(const CapabilitiesState& s) {
  CanFrame f = MakeFrame(kStateCapabilities);
  f.data[0] = s.version;
  f.data[1] = static_cast<std::uint8_t>(s.base_type);
  f.data[2] = s.dof_mask;
  f.data[3] = s.wheel_count;
  f.data[4] = s.model_profile_id;
  f.data[5] = s.model_profile_ver;
  StampE2e(f, s.counter);
  return f;
}
inline CanFrame Encode(const LimitsState& s) {
  CanFrame f = MakeFrame(kStateLimits);
  PutI16(f, 0, SatI16(s.max_vx * kVelScale));
  PutI16(f, 2, SatI16(s.max_vy * kVelScale));
  PutI16(f, 4, SatI16(s.max_wz * kAngVelScale));
  StampE2e(f, s.counter);
  return f;
}
inline CanFrame Encode(const BatteryState& s) {
  CanFrame f = MakeFrame(kStateBattery);
  PutU16(f, 0, SatU16(s.voltage * kVoltScale));
  PutI16(f, 2, SatI16(s.current * kAmpScale));
  f.data[4] = s.soc;
  StampE2e(f, s.counter);
  return f;
}
inline CanFrame Encode(const FaultState& s) {
  CanFrame f = MakeFrame(kStateFaults);
  PutU32(f, 0, s.faults);
  f.data[4] = s.last_fault;
  StampE2e(f, s.counter);
  return f;
}

// ---- decode (shape + E2E CRC gated) ---------------------------------------
inline std::optional<TwistCommand> DecodeTwist(const CanFrame& f) {
  if (!DecodableAs(f, kCmdTwist)) return std::nullopt;
  return TwistCommand{GetI16(f, 0) / kVelScale, GetI16(f, 2) / kVelScale,
                      GetI16(f, 4) / kAngVelScale, f.data[kCounterByte]};
}
inline std::optional<ModeCommand> DecodeMode(const CanFrame& f) {
  if (!DecodableAs(f, kCmdMode) || f.data[0] > 1) return std::nullopt;
  return ModeCommand{static_cast<ModeRequest>(f.data[0]), f.data[kCounterByte]};
}
inline std::optional<EStopCommand> DecodeEStop(const CanFrame& f) {
  if (!DecodableAs(f, kCmdEStop)) return std::nullopt;
  const std::uint8_t action = f.data[0];
  if (action != static_cast<std::uint8_t>(EStopAction::kEngage) &&
      action != static_cast<std::uint8_t>(EStopAction::kClear))
    return std::nullopt;
  EStopCommand c{static_cast<EStopAction>(action), GetU16(f, 1),
                 f.data[kCounterByte]};
  // Clear without the key is malformed by spec (§6.3): easy to engage, hard
  // to clear. Reject so no caller can act on it.
  if (c.action == EStopAction::kClear && c.key != kEStopClearKey)
    return std::nullopt;
  return c;
}
inline std::optional<HeartbeatCommand> DecodeHeartbeat(const CanFrame& f) {
  if (!DecodableAs(f, kCmdHeartbeat)) return std::nullopt;
  return HeartbeatCommand{f.data[0], f.data[kCounterByte]};
}
inline std::optional<StatusState> DecodeStatus(const CanFrame& f) {
  if (!DecodableAs(f, kStateStatus) || f.data[1] > 3) return std::nullopt;
  return StatusState{f.data[0], static_cast<ReportedMode>(f.data[1]), f.data[2],
                     GetU16(f, 3), f.data[kCounterByte]};
}
inline std::optional<OdomTwistState> DecodeOdomTwist(const CanFrame& f) {
  if (!DecodableAs(f, kStateOdomTwist)) return std::nullopt;
  return OdomTwistState{GetI16(f, 0) / kVelScale, GetI16(f, 2) / kVelScale,
                        GetI16(f, 4) / kAngVelScale, f.data[kCounterByte]};
}
inline std::optional<OdomPoseState> DecodeOdomPose(const CanFrame& f) {
  if (!DecodableAs(f, kStateOdomPose)) return std::nullopt;
  return OdomPoseState{GetI16(f, 0) / kPoseScale, GetI16(f, 2) / kPoseScale,
                       GetI16(f, 4) / kThetaScale, f.data[kCounterByte]};
}
inline std::optional<CapabilitiesState> DecodeCapabilities(const CanFrame& f) {
  if (!DecodableAs(f, kStateCapabilities)) return std::nullopt;
  return CapabilitiesState{f.data[0], static_cast<BaseType>(f.data[1]),
                           f.data[2], f.data[3], f.data[4], f.data[5],
                           f.data[kCounterByte]};
}
inline std::optional<LimitsState> DecodeLimits(const CanFrame& f) {
  if (!DecodableAs(f, kStateLimits)) return std::nullopt;
  return LimitsState{GetI16(f, 0) / kVelScale, GetI16(f, 2) / kVelScale,
                     GetI16(f, 4) / kAngVelScale, f.data[kCounterByte]};
}
inline std::optional<BatteryState> DecodeBattery(const CanFrame& f) {
  if (!DecodableAs(f, kStateBattery)) return std::nullopt;
  return BatteryState{GetU16(f, 0) / kVoltScale, GetI16(f, 2) / kAmpScale,
                      f.data[4], f.data[kCounterByte]};
}
inline std::optional<FaultState> DecodeFaults(const CanFrame& f) {
  if (!DecodableAs(f, kStateFaults)) return std::nullopt;
  return FaultState{GetU32(f, 0), f.data[4], f.data[kCounterByte]};
}

}  // namespace xmotion::mobile_base

#endif  // XMOTION_MOBILE_BASE_CORE_CODEC_HPP

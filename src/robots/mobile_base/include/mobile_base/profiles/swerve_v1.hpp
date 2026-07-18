/*
 * profiles/swerve_v1.hpp — Swerve Module reference profile (model_profile_id 1).
 *
 * A REFERENCE model-profile extension for swerve-drive bases, built entirely on
 * the core's public helpers (MakeFrame / PutI16 / GetI16 / StampE2e). It is
 * OPT-IN: include this header only on a swerve platform. It is versioned
 * independently of the core (kProfileVersion), so evolving it never changes the
 * core protocol version.
 *
 * Frames: STATE_MODULE[i] at 0x220+i (i in 0..3), in the model-state extension
 * range. The module index is carried by the CAN id. Module order is the
 * kinematics order: 0 front-right, 1 front-left, 2 rear-left, 3 rear-right.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */

#ifndef XMOTION_MOBILE_BASE_PROFILES_SWERVE_V1_HPP
#define XMOTION_MOBILE_BASE_PROFILES_SWERVE_V1_HPP

#include <cstdint>
#include <optional>

#include "mobile_base/codec_util.hpp"
#include "mobile_base/core.hpp"

namespace xmotion::mobile_base::profiles::swerve_v1 {

inline constexpr std::uint8_t kProfileId = 1;      // STATE_CAPABILITIES.model_profile_id
inline constexpr std::uint8_t kProfileVersion = 1;
inline constexpr std::uint32_t kStateModuleBase = 0x220;  // 0x220 + i
inline constexpr int kNumModules = 4;

static_assert(kStateModuleBase >= kStateExtBegin &&
                  kStateModuleBase + kNumModules <= kStateExtEnd,
              "swerve module frames must lie in the model-state extension range");

struct ModuleState {
  std::uint8_t index = 0;    // 0 FR, 1 FL, 2 RL, 3 RR (carried by the frame id)
  double speed = 0.0;        // wheel ground speed, m/s, signed
  double angle = 0.0;        // steering angle, rad
  std::uint8_t fault = 0;    // device-specific module fault byte, 0 = ok
  std::uint8_t counter = 0;  // E2E alive counter
};

inline std::uint32_t ModuleId(std::uint8_t index) {
  return kStateModuleBase + (index & 0x03u);
}

inline CanFrame Encode(const ModuleState& s) {
  CanFrame f = MakeFrame(ModuleId(s.index));
  PutI16(f, 0, SatI16(s.speed * kVelScale));
  PutI16(f, 2, SatI16(s.angle * kAngleScale));
  f.data[4] = s.fault;
  StampE2e(f, s.counter);
  return f;
}

inline std::optional<ModuleState> Decode(const CanFrame& f) {
  if (f.id < kStateModuleBase || f.id >= kStateModuleBase + kNumModules ||
      f.extended || f.remote || f.dlc != 8 || !CrcValid(f)) {
    return std::nullopt;
  }
  ModuleState s;
  s.index = static_cast<std::uint8_t>(f.id - kStateModuleBase);
  s.speed = GetI16(f, 0) / kVelScale;
  s.angle = GetI16(f, 2) / kAngleScale;
  s.fault = f.data[4];
  s.counter = f.data[kCounterByte];
  return s;
}

}  // namespace xmotion::mobile_base::profiles::swerve_v1

#endif  // XMOTION_MOBILE_BASE_PROFILES_SWERVE_V1_HPP

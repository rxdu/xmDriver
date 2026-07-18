/*
 * mobile_base.hpp — the abstract Mobile-Base interface (spec §6-§7 vocabulary).
 *
 * The robot-agnostic contract an upper layer (navigation) commands and reads,
 * independent of HOW the base is realized: body twist / mode / e-stop in;
 * status, odometry, capabilities, limits, battery, faults out. The frozen Core
 * Profile (mobile_base::*) is the shared vocabulary, so this interface is both a
 * wire format (when it crosses CAN to a firmware base) and an in-process C++
 * contract (when a host realizes it directly) — same semantics, two forms.
 *
 * Two realizations implement it:
 *   - MobileBaseCanClient : serializes the contract over CAN to a firmware base
 *     (e.g. swervebot).
 *   - a host-composed base (e.g. an Ackermann RC car mapping twist -> a bicycle
 *     model and driving VESC + a steering servo directly).
 *
 * Navigation depends on THIS type, so supporting a new base is a new
 * implementation, not a framework change. STATE_CAPABILITIES (base_type /
 * dof_mask / limits) lets a planner adapt to a base's kinematics by DATA, not by
 * branching on robot identity.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */

#ifndef XMOTION_MOBILE_BASE_INTERFACE_HPP
#define XMOTION_MOBILE_BASE_INTERFACE_HPP

#include "xmdriver/hal/device.hpp"
#include "xmdriver/hal/result.hpp"

#include "mobile_base/types.hpp"

namespace xmotion {

class MobileBase : public hal::Device {
 public:
  // ---- commands (upper layer -> base) -------------------------------------
  // vx/vy m/s, wz rad/s. A base realizes what its kinematics allow and reports
  // its DOF in ReadCapabilities(); an unrealizable axis (e.g. vy on Ackermann)
  // is simply not acted on.
  virtual hal::Status SetTwist(double vx, double vy, double wz) = 0;
  virtual hal::Status SetMode(mobile_base::ModeRequest mode) = 0;
  virtual hal::Status EngageEStop() = 0;
  virtual hal::Status ClearEStop() = 0;

  // ---- state (base -> upper layer); kTimeout if silent / not yet reporting --
  virtual hal::Result<mobile_base::StatusState> ReadStatus() const = 0;
  virtual hal::Result<mobile_base::OdomTwistState> ReadOdomTwist() const = 0;
  virtual hal::Result<mobile_base::OdomPoseState> ReadOdomPose() const = 0;
  virtual hal::Result<mobile_base::CapabilitiesState> ReadCapabilities() const = 0;
  virtual hal::Result<mobile_base::LimitsState> ReadLimits() const = 0;
  virtual hal::Result<mobile_base::BatteryState> ReadBattery() const = 0;
  virtual hal::Result<mobile_base::FaultState> ReadFaults() const = 0;
};

}  // namespace xmotion

#endif  // XMOTION_MOBILE_BASE_INTERFACE_HPP

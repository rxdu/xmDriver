/*
 * legacy_motor_adapter.hpp
 *
 * Bridges a new-HAL motor (hal::Motor + capability mixins) to the legacy
 * MotorControllerInterface, so existing consumers (e.g. xmNabla's actuator
 * groups) can drive new-HAL drivers unchanged during the migration (ADR 0001).
 *
 * The wrapped motor's lifecycle (Connect/Disconnect) is the caller's
 * responsibility — this adapter only forwards commands. A command whose
 * capability the underlying motor does not implement throws std::runtime_error,
 * matching the legacy interface's "not implemented" contract.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "xmmu/hal/motor_controller.hpp"            // new capability interfaces
#include "xmmu/hal/motor_controller_interface.hpp"  // legacy interface

namespace xmotion {

class LegacyMotorAdapter : public MotorControllerInterface {
 public:
  explicit LegacyMotorAdapter(std::shared_ptr<hal::Motor> motor)
      : motor_(std::move(motor)) {}

  void SetSpeed(float rpm) override {
    Speed().SetSpeed(hal::Rpm{rpm});
  }
  float GetSpeed() override {
    return static_cast<float>(Speed().GetSpeed().value_or(hal::Rpm{}).value());
  }

  void SetPosition(float position) override {
    Position().SetPosition(hal::Radian{position});
  }
  float GetPosition() override {
    return static_cast<float>(
        Position().GetPosition().value_or(hal::Radian{}).value());
  }

  void SetTorque(float torque) override {
    Torque().SetTorque(hal::NewtonMetre{torque});
  }
  float GetTorque() override {
    return static_cast<float>(
        Torque().GetTorque().value_or(hal::NewtonMetre{}).value());
  }

  void SetCurrent(float current) override {
    Current().SetCurrent(hal::Ampere{current});
  }
  float GetCurrent() override {
    return static_cast<float>(
        Current().GetCurrent().value_or(hal::Ampere{}).value());
  }

  void ApplyBrake(float brake) override { Brake().ApplyBrake(hal::Ratio{brake}); }
  void ReleaseBrake() override { Brake().ReleaseBrake(); }

  bool IsNormal() override {
    return motor_ && motor_->Health().state == hal::DeviceHealth::State::kOk;
  }

 private:
  // Capability accessors: return the mixin or throw (matches the legacy
  // interface, whose optional ops throw when unimplemented).
  template <typename Cap>
  Cap& As(const char* what) {
    auto* c = dynamic_cast<Cap*>(motor_.get());
    if (!c) throw std::runtime_error(std::string("LegacyMotorAdapter: ") + what +
                                     " not supported by this motor");
    return *c;
  }
  hal::SpeedControllable& Speed() {
    return As<hal::SpeedControllable>("speed control");
  }
  hal::PositionControllable& Position() {
    return As<hal::PositionControllable>("position control");
  }
  hal::TorqueControllable& Torque() {
    return As<hal::TorqueControllable>("torque control");
  }
  hal::CurrentControllable& Current() {
    return As<hal::CurrentControllable>("current control");
  }
  hal::Brakable& Brake() { return As<hal::Brakable>("braking"); }

  std::shared_ptr<hal::Motor> motor_;
};

}  // namespace xmotion

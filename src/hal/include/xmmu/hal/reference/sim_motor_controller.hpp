/*
 * sim_motor_controller.hpp
 *
 * Reference implementation of the new HAL motor interfaces — a hardware-free,
 * dependency-free simulated motor. It exists to (a) prove the interface set is
 * implementable and ergonomic, and (b) serve as a test double so upper-layer
 * code (e.g. xmNabla's actuator groups) can be exercised with no hardware.
 *
 * It demonstrates the contract the real drivers must follow: commands clamp to
 * the configured safe range and reject non-finite input; reads return Result;
 * Stop() forces a safe (zero) state; health reflects connection state.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <cmath>

#include "xmmu/hal/motor_controller.hpp"
#include "xmmu/hal/motor_factory.hpp"

namespace xmotion::hal {

class SimMotorController final : public Motor,
                                public SpeedControllable,
                                public CurrentControllable,
                                public Brakable {
 public:
  explicit SimMotorController(double max_speed_rpm = 5000.0,
                              double max_current_a = 20.0)
      : max_speed_rpm_(max_speed_rpm > 0 ? max_speed_rpm : 5000.0),
        max_current_a_(max_current_a > 0 ? max_current_a : 20.0) {}

  // --- Device ---
  Status Connect() override {
    connected_ = true;
    return Status::kOk;
  }
  void Disconnect() override {
    Stop();  // never leave a "spinning" sim behind
    connected_ = false;
  }
  bool IsConnected() const override { return connected_; }
  DeviceHealth Health() const override {
    return {connected_ ? DeviceHealth::State::kOk
                       : DeviceHealth::State::kDisconnected,
            ""};
  }

  // --- SafetyStoppable ---
  Status Stop() override {
    speed_rpm_ = 0.0;
    current_a_ = 0.0;
    return Status::kOk;
  }

  // --- SpeedControllable ---
  Status SetSpeed(Rpm speed) override {
    if (!connected_) return Status::kNotConnected;
    if (!std::isfinite(speed.value())) return Status::kInvalidArgument;
    const double v = speed.value();
    if (v < -max_speed_rpm_ || v > max_speed_rpm_) {
      speed_rpm_ = Clamp(v, -max_speed_rpm_, max_speed_rpm_);
      return Status::kOutOfRange;  // clamped AND reported (no silent saturation)
    }
    speed_rpm_ = v;
    return Status::kOk;
  }
  Result<Rpm> GetSpeed() override {
    if (!connected_) return Result<Rpm>::Err(Status::kNotConnected);
    return Result<Rpm>::Ok(Rpm{speed_rpm_});
  }

  // --- CurrentControllable ---
  Status SetCurrent(Ampere current) override {
    if (!connected_) return Status::kNotConnected;
    if (!std::isfinite(current.value())) return Status::kInvalidArgument;
    const double a = current.value();
    if (a < -max_current_a_ || a > max_current_a_) {
      current_a_ = Clamp(a, -max_current_a_, max_current_a_);
      return Status::kOutOfRange;
    }
    current_a_ = a;
    return Status::kOk;
  }
  Result<Ampere> GetCurrent() override {
    if (!connected_) return Result<Ampere>::Err(Status::kNotConnected);
    return Result<Ampere>::Ok(Ampere{current_a_});
  }

  // --- Brakable ---
  Status ApplyBrake(Ratio amount) override {
    if (!connected_) return Status::kNotConnected;
    const double r = amount.value();
    if (!std::isfinite(r) || r < 0.0 || r > 1.0) return Status::kOutOfRange;
    speed_rpm_ *= (1.0 - r);  // crude sim: brake bleeds speed
    return Status::kOk;
  }
  Status ReleaseBrake() override {
    return connected_ ? Status::kOk : Status::kNotConnected;
  }

 private:
  static double Clamp(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
  }

  double max_speed_rpm_;
  double max_current_a_;
  bool connected_ = false;
  double speed_rpm_ = 0.0;
  double current_a_ = 0.0;
};

// Self-registration with the factory under "sim" — apps get a working motor via
// MotorConfig{.type = "sim"} with no driver header included.
inline const MotorRegistrar kSimMotorRegistrar{
    "sim", [](const MotorConfig& c) -> std::unique_ptr<Motor> {
      return std::make_unique<SimMotorController>(c.max_speed_rpm,
                                                  c.max_current_a);
    }};

}  // namespace xmotion::hal

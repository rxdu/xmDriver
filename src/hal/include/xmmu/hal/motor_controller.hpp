/*
 * motor_controller.hpp
 *
 * Capability-segregated actuator interfaces (Interface Segregation Principle).
 * Instead of one fat MotorController that throws for what it doesn't support,
 * a driver implements only the capability mixins it actually has — so capability
 * lives in the type system, not in runtime exceptions.
 *
 *   Motor                = Device + SafetyStoppable   (every actuator)
 *   + SpeedControllable / PositionControllable / TorqueControllable /
 *     CurrentControllable / Brakable                  (as supported)
 *
 * An upper layer depends only on the mixin it needs and discovers extra
 * capabilities with dynamic_cast (the mixins are polymorphic):
 *
 *   if (auto* s = dynamic_cast<SpeedControllable*>(motor.get()))
 *     s->SetSpeed(Rpm{120});
 *
 * All commands return Status; all reads return Result<T>. Commands are expected
 * to clamp to the device's configured safe range and report kOutOfRange / reject
 * non-finite input — bounded motion is part of the contract.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include "xmmu/hal/device.hpp"
#include "xmmu/hal/result.hpp"
#include "xmmu/hal/units.hpp"

namespace xmotion::hal {

// Mandatory for EVERY actuator: bring it to a safe state on demand. This is the
// failsafe hook the old interface lacked; groups/supervisors/e-stop call it.
class SafetyStoppable {
 public:
  virtual ~SafetyStoppable() = default;
  virtual Status Stop() = 0;  // command a safe state (zero output / brake)
};

class SpeedControllable {
 public:
  virtual ~SpeedControllable() = default;
  virtual Status SetSpeed(Rpm speed) = 0;
  virtual Result<Rpm> GetSpeed() = 0;
};

class PositionControllable {
 public:
  virtual ~PositionControllable() = default;
  virtual Status SetPosition(Radian position) = 0;
  virtual Result<Radian> GetPosition() = 0;
};

class TorqueControllable {
 public:
  virtual ~TorqueControllable() = default;
  virtual Status SetTorque(NewtonMetre torque) = 0;
  virtual Result<NewtonMetre> GetTorque() = 0;
};

class CurrentControllable {
 public:
  virtual ~CurrentControllable() = default;
  virtual Status SetCurrent(Ampere current) = 0;
  virtual Result<Ampere> GetCurrent() = 0;
};

class Brakable {
 public:
  virtual ~Brakable() = default;
  virtual Status ApplyBrake(Ratio amount) = 0;  // amount in [0,1]
  virtual Status ReleaseBrake() = 0;
};

// The minimal actuator handle the factory hands back: connectable + stoppable.
// Concrete drivers also inherit the capability mixins they implement.
class Motor : public Device, public SafetyStoppable {};

}  // namespace xmotion::hal

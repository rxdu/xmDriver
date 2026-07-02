/*
 * @file motor_akelc.hpp
 * @date 4/20/24
 * @brief AKELC brushed-DC driver on the redesigned xmotion::hal interface.
 *
 * Wraps the low-level MotorAkelcModbus register access and adds the HAL
 * contract the legacy MotorControllerInterface path lacked: a real lifecycle
 * (Disconnect() commands Stop() before closing), commands that reject
 * non-finite input and clamp to the configured speed envelope (reporting
 * kOutOfRange), reads that return Result (never a fake 0), a real Stop()
 * failsafe, and a Health() driven by freshness + the device fault register.
 * Constructed through MotorFactory ("akelc"), so apps select it by config
 * without naming this type.
 *
 * The AKELC drives speed (RPM) and exposes a brake, so it implements
 * SpeedControllable + Brakable. It has no set-current register, so it is NOT
 * CurrentControllable (current is read-only telemetry, not a command).
 *
 * @copyright Copyright (c) 2024 Ruixiang Du (rdu)
 */

#ifndef XMOTION_MOTOR_AKELC_HPP_
#define XMOTION_MOTOR_AKELC_HPP_

#include <memory>
#include <string>

#include "xmmu/hal/motor_controller.hpp"
#include "xmmu/hal/motor_factory.hpp"
#include "xmmu/transport/modbus_rtu_interface.hpp"
#include "motor_akelc/motor_akelc_modbus.hpp"

namespace xmotion {

class MotorAkelc final : public hal::Motor,
                         public hal::SpeedControllable,
                         public hal::Brakable {
 public:
  struct Config {
    std::string bus;             // serial device, e.g. "/dev/ttyUSB0"
    int device_id = 1;           // Modbus slave id
    int baud_rate = 115200;      // AKELC default
    double max_speed_rpm = 0.0;  // |RPM| envelope; <=0 => default
    double max_current_a = 0.0;  // informational (no set-current register)
  };

  // Production path: owns a real Modbus-RTU port opened in Connect().
  explicit MotorAkelc(Config cfg);

  // Injection path (tests / custom transports): the caller supplies the
  // transport; Connect() opens it if needed but does not replace it.
  MotorAkelc(Config cfg, std::shared_ptr<ModbusRtuInterface> port);

  ~MotorAkelc() override;

  // --- hal::Device ---
  hal::Status Connect() override;
  void Disconnect() override;
  bool IsConnected() const override;
  hal::DeviceHealth Health() const override;

  // --- hal::SafetyStoppable ---
  hal::Status Stop() override;  // zero target RPM — the safe state

  // --- hal::SpeedControllable ---
  hal::Status SetSpeed(hal::Rpm speed) override;
  hal::Result<hal::Rpm> GetSpeed() override;

  // --- hal::Brakable ---
  hal::Status ApplyBrake(hal::Ratio amount) override;  // amount in [0,1]
  hal::Status ReleaseBrake() override;

 private:
  Config cfg_;
  std::shared_ptr<ModbusRtuInterface> port_;
  std::unique_ptr<MotorAkelcModbus> impl_;
  bool connected_ = false;
};

// Explicit registration for static-library linkage (a static self-registrar
// can be dropped by the linker if nothing else references this TU). Call once
// at startup, or link the driver with --whole-archive. Returns true if
// registered.
bool RegisterAkelcMotor();

}  // namespace xmotion

#endif  // XMOTION_MOTOR_AKELC_HPP_

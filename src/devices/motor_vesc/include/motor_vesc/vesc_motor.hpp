/*
 * vesc_motor.hpp
 *
 * VESC motor driver on the redesigned xmotion::hal interface (ADR 0001). It
 * wraps the existing VescCanInterface and adds the HAL contract the legacy path
 * lacked: commands clamp to a configured safe envelope and reject non-finite
 * input, reads return Result (never a fake 0 on error), and Stop() is a real
 * failsafe. Constructed through MotorFactory ("vesc"), so apps select it by
 * config without naming this type.
 *
 * Speed is the VESC's native electrical RPM (eRPM). Current is motor amps;
 * negative current is braking/regen and is permitted within the configured
 * envelope.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#ifndef XMMU_MOTOR_VESC_VESC_MOTOR_HPP
#define XMMU_MOTOR_VESC_VESC_MOTOR_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "xmmu/hal/motor_controller.hpp"
#include "xmmu/hal/motor_factory.hpp"
#include "xmmu/hal/freshness.hpp"
#include "motor_vesc/vesc_can_interface.hpp"

namespace xmotion {

class VescMotor final : public hal::Motor,
                        public hal::SpeedControllable,
                        public hal::CurrentControllable,
                        public hal::Brakable {
 public:
  struct Config {
    std::string bus;                  // CAN interface, e.g. "can0"
    std::uint8_t id = 0;              // VESC CAN id
    double max_speed_erpm = 0.0;      // |eRPM| envelope; <=0 => default
    double max_current_a = 0.0;       // |A| envelope; <=0 => default
    // How long since the last VESC status frame before feedback is stale. VESC
    // status frames are periodic; ~200 ms tolerates several missed frames.
    double freshness_ms = 200.0;      // <=0 => default
  };

  explicit VescMotor(Config cfg);
  // Transport-injecting ctor (tests / shared-bus setups). If `can` is null,
  // Connect() builds an AsyncCAN from cfg.bus, matching the production path.
  VescMotor(Config cfg, std::shared_ptr<CanInterface> can);

  // --- hal::Device ---
  hal::Status Connect() override;
  void Disconnect() override;
  bool IsConnected() const override;
  hal::DeviceHealth Health() const override;

  // --- hal::SafetyStoppable ---
  hal::Status Stop() override;  // zero current (coast) — safe state

  // --- hal::SpeedControllable ---
  hal::Status SetSpeed(hal::Rpm speed) override;
  hal::Result<hal::Rpm> GetSpeed() override;

  // --- hal::CurrentControllable ---
  hal::Status SetCurrent(hal::Ampere current) override;
  hal::Result<hal::Ampere> GetCurrent() override;

  // --- hal::Brakable ---
  hal::Status ApplyBrake(hal::Ratio amount) override;  // amount in [0,1]
  hal::Status ReleaseBrake() override;

 private:
  Config cfg_;
  // Non-null only when a transport was injected via the test/shared-bus ctor;
  // Connect() hands it to VescCanInterface instead of opening cfg_.bus.
  std::shared_ptr<CanInterface> injected_can_;
  VescCanInterface vesc_;
  // Marked on every VESC status frame; drives kTimeout reads and stale Health.
  hal::FreshnessMonitor monitor_;
  bool connected_ = false;
  // Set from the transport error callback (I/O thread) on an async bus fault so
  // Health() reports kFault instead of only a stale/degraded read. Atomic
  // because it is written off the control thread.
  std::atomic<bool> bus_fault_{false};
};

// Explicit registration for static-library linkage (a static self-registrar can
// be dropped by the linker if nothing else references this TU). Call once at
// startup, or link the driver with --whole-archive. Returns true if registered.
bool RegisterVescMotor();

}  // namespace xmotion

#endif  // XMMU_MOTOR_VESC_VESC_MOTOR_HPP

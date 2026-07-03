/*
 * sms_sts_servo.hpp
 *
 * Waveshare SMS/STS bus servo on the redesigned xmotion::hal interface
 * (ADR 0001/0003). Wraps the vendored SCServo protocol library and adds the HAL
 * contract: commands clamp to the configured envelope and reject non-finite
 * input, Stop() commands a safe state, Disconnect() commands it before closing,
 * and Health() is derived from freshness + the servo's reported voltage/temp/
 * status. Constructed through MotorFactory ("sms_sts").
 *
 * The servo bus is half-duplex and the vendored protocol is synchronous, so a
 * background thread polls feedback into a cached, timestamped snapshot; the
 * public reads (GetPosition/GetSpeed/GetState) copy that snapshot and NEVER
 * block the caller or sleep per-servo on the read path. Reads return Result<T>
 * gated by a FreshnessMonitor.
 *
 * Position uses radians at the HAL boundary, mapped to the servo's 0..360 deg /
 * 0..4095 step range. Speed uses the servo's native step/sec carried in the Rpm
 * scalar (|2400| envelope by default), matching how WriteSpe/ReadSpeed report.
 *
 * @copyright Copyright (c) 2024 Ruixiang Du (rdu)
 */

#ifndef XMMU_MOTOR_WAVESHARE_SMS_STS_SERVO_HPP
#define XMMU_MOTOR_WAVESHARE_SMS_STS_SERVO_HPP

#include <cstdint>
#include <memory>
#include <string>

#include "xmdriver/hal/motor_controller.hpp"
#include "xmdriver/hal/motor_factory.hpp"

namespace xmotion {

class SmsStsServo final : public hal::Motor,
                          public hal::PositionControllable,
                          public hal::SpeedControllable {
 public:
  enum class Mode { kSpeed = 0, kPosition, kUnknown = 0xff };

  struct Config {
    std::string bus;                   // serial device, e.g. "/dev/ttyUSB0"
    std::uint8_t id = 1;               // servo id
    int baud = 1000000;                // bus baud rate
    double max_speed = 0.0;            // |step/sec| envelope; <=0 => default 2400
    double position_offset_deg = 0.0;  // command/feedback remap offset (deg)
  };

  // Latest feedback snapshot (all fields are the last successfully polled value).
  struct State {
    bool is_moving = false;
    double position = 0.0;     // deg (offset-corrected)
    double speed = 0.0;        // step/sec
    double load = 0.0;
    double voltage = 0.0;      // 0.1 V units as reported by the servo
    double temperature = 0.0;  // deg C
    double current = 0.0;
  };

  explicit SmsStsServo(Config cfg);
  ~SmsStsServo() override;

  // do not allow copy
  SmsStsServo(const SmsStsServo&) = delete;
  SmsStsServo& operator=(const SmsStsServo&) = delete;

  // --- hal::Device ---
  hal::Status Connect() override;
  void Disconnect() override;
  bool IsConnected() const override;
  hal::DeviceHealth Health() const override;

  // --- hal::SafetyStoppable ---
  hal::Status Stop() override;  // command zero speed — the safe state

  // --- hal::PositionControllable ---
  hal::Status SetPosition(hal::Radian position) override;
  hal::Result<hal::Radian> GetPosition() override;

  // --- hal::SpeedControllable ---
  hal::Status SetSpeed(hal::Rpm speed) override;
  hal::Result<hal::Rpm> GetSpeed() override;

  // Latest cached feedback snapshot; Err(kTimeout) if stale, Err(kNotConnected)
  // if down. Never blocks.
  hal::Result<State> GetState() const;

  // Config-time handshakes. BLOCKING; do not call on the control hot path.
  hal::Status SetMode(Mode mode);
  hal::Status SetMotorId(uint8_t id);
  hal::Status SetNeutralPosition();

 private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

// Explicit registration for static-library linkage. Returns true.
bool RegisterSmsStsServo();

}  // namespace xmotion

#endif  // XMMU_MOTOR_WAVESHARE_SMS_STS_SERVO_HPP

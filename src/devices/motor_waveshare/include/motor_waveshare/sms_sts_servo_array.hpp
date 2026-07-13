/*
 * sms_sts_servo_array.hpp
 *
 * Waveshare SMS/STS bus servos sharing ONE serial bus, on the redesigned
 * xmotion::hal interface (ADR 0001/0003). The SCS protocol is id-multiplexed
 * on a single half-duplex line and the vendored library is synchronous, so
 * the honest model is ONE owner of the port that serializes every bus
 * transaction and keeps per-id cached state — not N independent driver
 * instances each opening the same device node with its own poll thread.
 *
 * DESIGN DECISION — array coordinator vs shared-port single instances:
 * this class is an ARRAY COORDINATOR. It owns one vendored SMS_STS protocol
 * instance (one fd, one bus mutex) and one background poll thread that
 * round-robins FeedBack(id) over the configured ids into per-id, timestamped
 * snapshots. The alternative (N SmsStsServo instances on the same device)
 * was rejected: each instance would open the tty separately and run its own
 * poll thread, interleaving half-duplex transactions mid-frame. The
 * single-device SmsStsServo stays untouched for the one-servo-per-port case;
 * this class reuses its State snapshot type and the same HAL primitives
 * (Status/Result, FreshnessMonitor, HealthReporter).
 *
 * HAL shape: the array is a hal::Motor (Device + SafetyStoppable) so it can
 * be constructed through MotorFactory ("sms_sts_array", ids in params["ids"]
 * as a comma-separated list) and Stop() is the failsafe fan-out over every
 * id. Per-id verbs mirror the single-device contract (clamp + reject
 * non-finite, Result-gated reads off the cached snapshot — reads never block
 * on the bus). Member(id) returns a per-id capability view implementing
 * PositionControllable + SpeedControllable + SafetyStoppable for hal actuator
 * group composition.
 *
 * Batch semantics: SetPositions() uses the protocol's sync-write
 * (SyncWritePosEx, broadcast id 0xfe) — genuinely ONE bus transaction for all
 * ids, no per-servo acknowledgement. The whole vector is validated before
 * anything is sent: any non-finite entry or a size mismatch commands nothing.
 *
 * Provisioning (SetMotorId / SetNeutralPosition / mode changes) is
 * deliberately NOT part of the array: those are one-servo-on-the-bench tasks
 * — use the single-device SmsStsServo driver for them.
 *
 * Units match SmsStsServo: position uses radians at the HAL boundary, mapped
 * to the servo's 0..360 deg / 0..4095 step range with a uniform
 * position_offset_deg remap; speed uses the servo's native step/sec carried
 * in the Rpm scalar (|2400| envelope by default).
 *
 * Per-id staleness: the poll thread visits ids sequentially and an absent id
 * costs a full IO timeout per round, so the per-id freshness window scales
 * with the id count (base window + one IO timeout per id). Health(id) reports
 * an absent/unresponsive servo as kDegraded ("no data yet"/"stale") while the
 * responsive ids stay kOk.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#ifndef XMMU_MOTOR_WAVESHARE_SMS_STS_SERVO_ARRAY_HPP
#define XMMU_MOTOR_WAVESHARE_SMS_STS_SERVO_ARRAY_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "xmdriver/hal/motor_controller.hpp"
#include "xmdriver/hal/motor_factory.hpp"

#include "motor_waveshare/sms_sts_servo.hpp"

namespace xmotion {

class SmsStsServoArray final : public hal::Motor {
 public:
  // Latest per-id feedback snapshot; same shape as the single-device driver.
  using State = SmsStsServo::State;

  struct Config {
    std::string bus;                   // serial device, e.g. "/dev/ttyUSB0"
    std::vector<std::uint8_t> ids;     // servo ids; order defines batch order
    int baud = 1000000;                // bus baud rate
    double max_speed = 0.0;            // |step/sec| envelope; <=0 => 2400
    double position_offset_deg = 0.0;  // uniform command/feedback remap (deg)
    // Per-transaction IO timeout of the vendored protocol (ms). The default
    // matches the vendored library; tests use a small value so an absent id
    // does not stall the poll round for 100 ms.
    int io_timeout_ms = 100;
  };

  // Per-id capability view for hal composition (actuator groups). Obtained
  // via Member(id); valid for the array's lifetime. Forwards to the owning
  // array; not independently connectable.
  class MemberRef final : public hal::PositionControllable,
                          public hal::SpeedControllable,
                          public hal::SafetyStoppable {
   public:
    hal::Status SetPosition(hal::Radian position) override;
    hal::Result<hal::Radian> GetPosition() override;
    hal::Status SetSpeed(hal::Rpm speed) override;
    hal::Result<hal::Rpm> GetSpeed() override;
    hal::Status Stop() override;
    std::uint8_t id() const { return id_; }

   private:
    friend class SmsStsServoArray;
    MemberRef(SmsStsServoArray* owner, std::uint8_t id)
        : owner_(owner), id_(id) {}
    SmsStsServoArray* owner_;
    std::uint8_t id_;
  };

  explicit SmsStsServoArray(Config cfg);
  ~SmsStsServoArray() override;

  // do not allow copy
  SmsStsServoArray(const SmsStsServoArray&) = delete;
  SmsStsServoArray& operator=(const SmsStsServoArray&) = delete;

  // --- hal::Device (the bus as one unit) ---
  // Connect validates the id list (non-empty, no duplicates, no broadcast id)
  // and opens the shared port; kInvalidArgument on a bad id list. An absent
  // servo does NOT fail Connect — it surfaces through per-id Health().
  hal::Status Connect() override;
  void Disconnect() override;  // failsafe Stop() fan-out, then close the port
  bool IsConnected() const override;
  hal::DeviceHealth Health() const override;  // worst member health

  // --- hal::SafetyStoppable ---
  // Failsafe fan-out: zero speed to EVERY id, in registration order, even
  // after individual failures; first failing status returned.
  hal::Status Stop() override;

  // --- per-id verbs (kInvalidArgument for an id not in cfg.ids) ---
  hal::Status SetPosition(std::uint8_t id, hal::Radian position);
  hal::Result<hal::Radian> GetPosition(std::uint8_t id);
  hal::Status SetSpeed(std::uint8_t id, hal::Rpm speed);
  hal::Result<hal::Rpm> GetSpeed(std::uint8_t id);
  hal::Result<State> GetState(std::uint8_t id) const;
  hal::DeviceHealth Health(std::uint8_t id) const;
  hal::Status Stop(std::uint8_t id);
  // Torque stage on/off (config-time; a disabled servo goes limp).
  hal::Status EnableTorque(std::uint8_t id, bool enable);
  hal::Status EnableTorqueAll(bool enable);

  // --- batch verb: ONE sync-write bus transaction ---
  // positions[i] goes to ids()[i]. `positions.size()` must equal
  // ids().size() and every entry must be finite; otherwise nothing is
  // commanded (kInvalidArgument). Out-of-range entries are clamped to the
  // servo envelope and reported as kOutOfRange (still commanded). Note: the
  // protocol's sync-write is unacknowledged, so kOk means "queued on the
  // wire", not "accepted by every servo" — feedback/health confirm execution.
  hal::Status SetPositions(const std::vector<hal::Radian>& positions);

  const std::vector<std::uint8_t>& ids() const;
  // Per-id capability view; nullptr for an unknown id.
  MemberRef* Member(std::uint8_t id);

 private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
  std::vector<std::unique_ptr<MemberRef>> members_;  // fixed after ctor
};

// Explicit registration for static-library linkage. Registers type
// "sms_sts_array"; MotorConfig.params["ids"] carries the id list as a
// comma-separated string (e.g. "1,2,3,4"); params["baud"] and
// params["position_offset_deg"] are also honored. Returns true.
bool RegisterSmsStsServoArray();

}  // namespace xmotion

#endif  // XMMU_MOTOR_WAVESHARE_SMS_STS_SERVO_ARRAY_HPP

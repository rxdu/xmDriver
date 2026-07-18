/*
 * ddsm_210_array.hpp
 *
 * Waveshare DDSM-210 hub motors sharing ONE serial bus, on the redesigned
 * xmotion::hal interface (ADR 0001/0003). The DDSM protocol is id-multiplexed
 * on a single half-duplex RS-485 line: every frame carries the motor id, so
 * the honest model is ONE owner of the port that serializes TX and demuxes RX
 * per id — not N independent driver instances fighting over one callback slot
 * and one file descriptor.
 *
 * DESIGN DECISION — array coordinator vs shared-port single instances:
 * this class is an ARRAY COORDINATOR. It owns the AsyncSerial transport, keeps
 * one RX ring buffer + frame scanner, and maintains per-id feedback/freshness
 * channels. The alternative (N Ddsm210 instances handed the same
 * SerialInterface) was rejected: AsyncSerial has a single receive-callback
 * slot (last registrant wins), each instance would re-parse the full byte
 * stream and count other ids' frames as errors (misleading telemetry), and
 * Disconnect() of any one member would close the shared port under the others.
 * The single-device Ddsm210 class stays untouched for the one-motor-per-port
 * case; this class reuses the same protocol codec (Ddsm210Frame) and the same
 * HAL primitives (Status/Result, FreshnessMonitor, HealthReporter).
 *
 * HAL shape: the array is a hal::Motor (Device + SafetyStoppable) so it can be
 * constructed through MotorFactory ("ddsm210_array", ids in params["ids"] as a
 * comma-separated list) and Stop() is the failsafe fan-out over every id.
 * Per-id command/read verbs mirror the single-device contract (clamp to the
 * safe envelope, reject non-finite input, Result-gated reads with per-id
 * freshness). Member(id) returns a per-id capability view implementing
 * SpeedControllable + PositionControllable + SafetyStoppable, so upper layers
 * can compose ids into hal actuator groups exactly like standalone motors.
 *
 * Batch semantics: the protocol has no multi-id frame, so SetSpeeds() queues
 * one command frame per id, back-to-back in registration order — one bus
 * transaction round. Group fan-out rules apply: every id is commanded even if
 * an earlier one fails (first failing status returned) and a size mismatch
 * commands nothing (kInvalidArgument).
 *
 * Provisioning (SetMotorId / SetMode) is deliberately NOT part of the array:
 * changing a motor id or mode is a one-motor-on-the-bench task — use the
 * single-device Ddsm210 driver for it.
 *
 * Units match Ddsm210: speed is native RPM (|210| envelope by default),
 * position is a shaft angle in radians mapped to the device's 0..360 deg
 * absolute range.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#ifndef XMMU_MOTOR_WAVESHARE_DDSM_210_ARRAY_HPP
#define XMMU_MOTOR_WAVESHARE_DDSM_210_ARRAY_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <chrono>
#include <string>
#include <vector>

#include "xmbase/telemetry/telemetry.hpp"
#include "xmdriver/hal/freshness.hpp"
#include "xmdriver/hal/health_telemetry.hpp"
#include "xmdriver/hal/motor_controller.hpp"
#include "xmdriver/hal/motor_factory.hpp"
#include "xmdriver/transport/serial_interface.hpp"

#include "async_port/ring_buffer.hpp"
#include "motor_waveshare/details/ddsm_210_frame.hpp"

namespace xmotion {

class Ddsm210Array final : public hal::Motor {
 public:
  using Mode = Ddsm210Frame::Mode;

  struct Config {
    std::string bus;                // serial device, e.g. "/dev/ttyUSB0"
    std::vector<std::uint8_t> ids;  // motor ids; order defines batch order
    double max_speed_rpm = 0.0;     // shared |RPM| envelope; <=0 => 210
    // How long a per-id feedback sample stays "fresh" before GetSpeed()/
    // GetState() report kTimeout. Widen it when several motors share the
    // half-duplex bus and feedback is polled round-robin, so an occasional
    // dropped reply between a motor's polls is tolerated.
    std::chrono::milliseconds feedback_timeout{200};
  };

  // Latest per-id feedback snapshot (last successfully decoded values).
  struct State {
    double speed_rpm = 0.0;
    double position_rad = 0.0;
    int32_t encoder_count = 0;
    double current_raw = 0.0;     // device-native current units (uncalibrated)
    double temperature_c = 0.0;
    std::uint8_t error_code = 0;  // device fault bits; 0 = normal
  };

  // Per-id capability view for hal composition (actuator groups, capability-
  // typed upper layers). Obtained via Member(id); valid for the array's
  // lifetime. Forwards to the owning array; not independently connectable.
  class MemberRef final : public hal::SpeedControllable,
                          public hal::PositionControllable,
                          public hal::SafetyStoppable {
   public:
    hal::Status SetSpeed(hal::Rpm speed) override;
    hal::Result<hal::Rpm> GetSpeed() override;
    hal::Status SetPosition(hal::Radian position) override;
    hal::Result<hal::Radian> GetPosition() override;
    hal::Status Stop() override;
    std::uint8_t id() const { return id_; }

   private:
    friend class Ddsm210Array;
    MemberRef(Ddsm210Array* owner, std::uint8_t id) : owner_(owner), id_(id) {}
    Ddsm210Array* owner_;
    std::uint8_t id_;
  };

  explicit Ddsm210Array(Config cfg);
  // Transport-injecting ctor (tests / pre-built ports). If `serial` is null,
  // Connect() builds an AsyncSerial from cfg.bus.
  Ddsm210Array(Config cfg, std::shared_ptr<SerialInterface> serial);
  ~Ddsm210Array() override;

  // do not allow copy
  Ddsm210Array(const Ddsm210Array&) = delete;
  Ddsm210Array& operator=(const Ddsm210Array&) = delete;

  // --- hal::Device (the bus as one unit) ---
  // Connect validates the id list (non-empty, no duplicates, no broadcast id)
  // and opens the shared port. kInvalidArgument on a bad id list.
  hal::Status Connect() override;
  void Disconnect() override;  // failsafe Stop() fan-out, then close the port
  bool IsConnected() const override;
  hal::DeviceHealth Health() const override;  // worst member health

  // --- hal::SafetyStoppable ---
  // Failsafe fan-out: zero speed to EVERY id, in registration order, even
  // after individual failures; first failing status returned.
  hal::Status Stop() override;

  // --- per-id verbs (kInvalidArgument for an id not in cfg.ids) ---
  hal::Status SetSpeed(std::uint8_t id, hal::Rpm speed);
  hal::Result<hal::Rpm> GetSpeed(std::uint8_t id);
  hal::Status SetPosition(std::uint8_t id, hal::Radian position);
  hal::Result<hal::Radian> GetPosition(std::uint8_t id);
  hal::Result<State> GetState(std::uint8_t id) const;
  hal::DeviceHealth Health(std::uint8_t id) const;
  hal::Status Stop(std::uint8_t id);

  // --- batch verb: one bus transaction round ---
  // speeds[i] goes to ids()[i]. `speeds.size()` must equal ids().size(); on
  // mismatch nothing is commanded and kInvalidArgument is returned. Every id
  // is commanded even if an earlier one fails (first failing status wins).
  hal::Status SetSpeeds(const std::vector<hal::Rpm>& speeds);

  // --- extras (not part of the HAL contract) ---
  // Ask a motor (or all) to push a feedback frame. Non-blocking; replies land
  // asynchronously and are served by the Get*() reads. Poll these on a timer
  // to keep odometry (position/encoder/error-code) fresh.
  void RequestOdometryFeedback(std::uint8_t id);
  void RequestOdometryFeedbackAll();
  void RequestModeFeedback(std::uint8_t id);
  Mode GetMode(std::uint8_t id) const;
  int32_t GetEncoderCount(std::uint8_t id) const;

  const std::vector<std::uint8_t>& ids() const { return cfg_.ids; }
  // Per-id capability view; nullptr for an unknown id.
  MemberRef* Member(std::uint8_t id);

 private:
  struct Channel {
    Channel(std::uint8_t motor_id, std::chrono::milliseconds freshness_timeout);
    std::uint8_t id;
    Ddsm210Frame::RawFeedback fb;  // guarded by snap_mtx_
    hal::FreshnessMonitor freshness;
  };

  Channel* FindChannel(std::uint8_t id) const;
  hal::DeviceHealth ChannelHealth(const Channel& ch) const;
  void ProcessRx(uint8_t* data, size_t bufsize, size_t len);
  hal::Status SendFrame(Ddsm210Frame& frame);
  hal::Status SendZeroSpeed(std::uint8_t id);
  hal::Status ValidateIds() const;

  Config cfg_;
  std::shared_ptr<SerialInterface> serial_;
  std::atomic<bool> connected_{false};
  RingBuffer<uint8_t, 2048> rx_buffer_;
  std::vector<std::unique_ptr<Channel>> channels_;    // fixed after ctor
  std::vector<std::unique_ptr<MemberRef>> members_;   // fixed after ctor
  mutable std::mutex snap_mtx_;  // guards every Channel::fb snapshot
  bool warned_unknown_id_ = false;  // IO-thread only; one warn per connect

  // Telemetry (observability only; handles registered once, no-op when no
  // backend is bound). Names are per-class, not per-port/per-id, by the
  // low-cardinality convention. Mutable members are touched from const
  // Health().
  telemetry::EventSource src_ =
      telemetry::GetEventSource("driver.ddsm210_array");
  mutable telemetry::Gauge data_age_ms_ =
      telemetry::GetGauge("driver.ddsm210_array.data_age_ms");
  telemetry::Counter serial_fault_metric_ =
      telemetry::GetCounter("driver.ddsm210_array.serial_fault_count");
  // Bytes discarded while re-synchronizing on an invalid RX frame.
  telemetry::Counter frame_error_metric_ =
      telemetry::GetCounter("driver.ddsm210_array.frame_error_count");
  // Valid frames whose motor id is not in cfg.ids (mis-wired bus / stray id).
  telemetry::Counter unknown_id_metric_ =
      telemetry::GetCounter("driver.ddsm210_array.unknown_id_count");
  mutable hal::HealthReporter health_reporter_{"driver.ddsm210_array"};
};

// Explicit registration for static-library linkage (a static self-registrar
// can be dropped by the linker if nothing else references this TU). Registers
// type "ddsm210_array"; MotorConfig.params["ids"] carries the id list as a
// comma-separated string (e.g. "1,2,3,4"). Returns true.
bool RegisterDdsm210Array();

}  // namespace xmotion

#endif  // XMMU_MOTOR_WAVESHARE_DDSM_210_ARRAY_HPP

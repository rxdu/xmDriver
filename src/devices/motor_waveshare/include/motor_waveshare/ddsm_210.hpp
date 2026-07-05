/*
 * ddsm_210.hpp
 *
 * Waveshare DDSM-210 hub motor on the redesigned xmotion::hal interface
 * (ADR 0001/0003). It drives the motor over an AsyncSerial transport and adds
 * the HAL contract the legacy MotorControllerInterface lacked: commands clamp to
 * a configured safe envelope and reject non-finite input, reads return Result
 * (never a fake 0 on error) gated by a FreshnessMonitor, Health() is derived
 * from freshness + the device error code, Stop() is a real failsafe and
 * Disconnect() commands it before closing the port. Constructed through
 * MotorFactory ("ddsm210").
 *
 * Speed is the motor's native RPM (|210| envelope by default). Position is a
 * shaft angle: the HAL uses radians, mapped to the device's 0..360 deg absolute
 * range. Position control requires the motor to be in position mode and speed
 * control requires speed mode (config-time, via SetMode()).
 *
 * @copyright Copyright (c) 2024 Ruixiang Du (rdu)
 */

#ifndef XMMU_MOTOR_WAVESHARE_DDSM_210_HPP
#define XMMU_MOTOR_WAVESHARE_DDSM_210_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

#include "xmbase/telemetry/telemetry.hpp"
#include "xmdriver/hal/freshness.hpp"
#include "xmdriver/hal/health_telemetry.hpp"
#include "xmdriver/hal/motor_controller.hpp"
#include "xmdriver/hal/motor_factory.hpp"
#include "xmdriver/transport/serial_interface.hpp"

#include "async_port/ring_buffer.hpp"
#include "motor_waveshare/details/ddsm_210_frame.hpp"

namespace xmotion {

class Ddsm210 final : public hal::Motor,
                      public hal::SpeedControllable,
                      public hal::PositionControllable {
 public:
  using Mode = Ddsm210Frame::Mode;

  struct Config {
    std::string bus;              // serial device, e.g. "/dev/ttyUSB0"
    std::uint8_t id = 1;          // motor id
    double max_speed_rpm = 0.0;   // |RPM| envelope; <=0 => default (210)
  };

  explicit Ddsm210(Config cfg);
  // Transport-injecting ctor (tests / shared-bus setups). If `serial` is null,
  // Connect() builds an AsyncSerial from cfg.bus.
  Ddsm210(Config cfg, std::shared_ptr<SerialInterface> serial);
  ~Ddsm210() override;

  // do not allow copy
  Ddsm210(const Ddsm210&) = delete;
  Ddsm210& operator=(const Ddsm210&) = delete;

  // --- hal::Device ---
  hal::Status Connect() override;
  void Disconnect() override;
  bool IsConnected() const override;
  hal::DeviceHealth Health() const override;

  // --- hal::SafetyStoppable ---
  hal::Status Stop() override;  // command zero speed — the safe state

  // --- hal::SpeedControllable ---
  hal::Status SetSpeed(hal::Rpm speed) override;
  hal::Result<hal::Rpm> GetSpeed() override;

  // --- hal::PositionControllable ---
  hal::Status SetPosition(hal::Radian position) override;
  hal::Result<hal::Radian> GetPosition() override;

  // --- extras (not part of the HAL contract) ---
  // Request that the motor push a feedback frame. Non-blocking; the reply lands
  // asynchronously on the RX path and is served by GetSpeed()/GetPosition().
  void RequestOdometryFeedback();
  void RequestModeFeedback();
  Mode GetMode() const;
  int32_t GetEncoderCount() const;

  // Config-time handshakes. These BLOCK the calling thread up to `timeout_ms`
  // polling for confirmation and MUST NOT be called on the control hot path
  // (they are not used by SetSpeed/SetPosition/Stop). Set id/mode once at setup.
  hal::Status SetMode(Mode mode, uint32_t timeout_ms = 100);
  hal::Status SetMotorId(uint8_t id, uint32_t timeout_ms = 100);

 private:
  void ProcessFeedback(uint8_t* data, size_t bufsize, size_t len);
  hal::Status SendFrame(Ddsm210Frame& frame);

  Config cfg_;
  std::shared_ptr<SerialInterface> serial_;
  std::atomic<bool> connected_{false};
  RingBuffer<uint8_t, 1024> rx_buffer_;
  Ddsm210Frame::RawFeedback raw_feedback_;
  hal::FreshnessMonitor freshness_;
  std::atomic<bool> id_set_ack_received_{false};

  // Telemetry (observability only; handles registered once, no-op when no
  // backend is bound). Mutable members are touched from const Health().
  telemetry::EventSource src_ = telemetry::GetEventSource("driver.ddsm210");
  mutable telemetry::Gauge data_age_ms_ =
      telemetry::GetGauge("driver.ddsm210.data_age_ms");
  telemetry::Counter serial_fault_metric_ =
      telemetry::GetCounter("driver.ddsm210.serial_fault_count");
  // Counts bytes discarded while re-synchronizing on an invalid RX frame.
  telemetry::Counter frame_error_metric_ =
      telemetry::GetCounter("driver.ddsm210.frame_error_count");
  mutable hal::HealthReporter health_reporter_{"driver.ddsm210"};
};

// Explicit registration for static-library linkage (a static self-registrar can
// be dropped by the linker if nothing else references this TU). Returns true.
bool RegisterDdsm210Motor();

}  // namespace xmotion

#endif  // XMMU_MOTOR_WAVESHARE_DDSM_210_HPP

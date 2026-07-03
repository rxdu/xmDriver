/*
 * imu_hipnuc.hpp
 *
 * HiPNUC serial IMU (CH/HI-series) driver on the redesigned xmotion::hal
 * interface (ADR 0001 + 0003). It wraps the vendored ch_serial.c frame parser
 * over an AsyncSerial transport and adds the HAL contract the legacy
 * ImuInterface lacked:
 *   - transport params (serial device + baud) are supplied at CONSTRUCTION, not
 *     baked into Connect();
 *   - Read() returns Result<ImuSample>, so a dead/stalled sensor is kTimeout and
 *     a never-connected one is kNotConnected — never a stale sample dressed as a
 *     live reading;
 *   - freshness is tracked with FreshnessMonitor (the HiPNUC streams at ~100 Hz;
 *     a sample older than the freshness window is reported stale);
 *   - Health() degrades on staleness and faults on transport link loss.
 *
 * The sensor is receive-only in this driver: it streams frames continuously, so
 * there is no command path. SetSampleCallback() provides the push path, invoked
 * on the transport RX thread for each fresh frame.
 *
 * Copyright (c) 2021-2026 Ruixiang Du (rdu)
 */

#ifndef XMMU_SENSOR_IMU_IMU_HIPNUC_HPP
#define XMMU_SENSOR_IMU_IMU_HIPNUC_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include "xmdriver/hal/freshness.hpp"
#include "xmdriver/hal/imu.hpp"
#include "xmdriver/transport/serial_interface.hpp"

namespace xmotion {

class ImuHipnuc final : public hal::Imu {
 public:
  struct Config {
    std::string device;              // serial port, e.g. "/dev/ttyUSB0"
    std::uint32_t baud_rate = 115200;
    // Staleness window: HiPNUC streams at ~100 Hz, so ~50 ms tolerates a few
    // missed frames before Read() reports kTimeout.
    std::chrono::milliseconds freshness_timeout{50};
  };

  explicit ImuHipnuc(Config cfg);
  // Injection constructor for tests: use a pre-built transport (e.g. a fake)
  // instead of opening a real AsyncSerial. Connect() drives the given transport.
  ImuHipnuc(Config cfg, std::shared_ptr<SerialInterface> serial);
  ~ImuHipnuc() override;

  // --- hal::Device ---
  hal::Status Connect() override;
  void Disconnect() override;
  bool IsConnected() const override;
  hal::DeviceHealth Health() const override;

  // --- hal::Imu ---
  hal::Result<hal::ImuSample> Read() override;
  void SetSampleCallback(SampleCallback cb) override;

 private:
  // Serial RX callback: feeds bytes to the HiPNUC parser and, on each decoded
  // frame, publishes a fresh ImuSample. Runs on the transport I/O thread.
  void OnSerialData(std::uint8_t *data, std::size_t bufsize, std::size_t len);
  // Serial error callback: link loss / read fault. Runs on the I/O thread.
  void OnSerialError(TransportStatus status);

  Config cfg_;
  std::shared_ptr<SerialInterface> serial_;

  // Latest decoded sample (Read() copies this out); guarded because it is
  // written on the RX thread and read by callers.
  mutable std::mutex data_mtx_;
  hal::ImuSample last_sample_;

  // Push-path callback, invoked on the RX thread for each fresh frame.
  SampleCallback sample_cb_;

  hal::FreshnessMonitor monitor_;
  std::atomic<bool> faulted_{false};
  std::atomic<std::uint64_t> parse_error_count_{0};

  // Per-instance HiPNUC parser accumulator (hipnuc_raw_t*, opaque to avoid
  // leaking the vendored ch_serial.h into this public header).
  void *raw_state_ = nullptr;
};

}  // namespace xmotion

#endif  // XMMU_SENSOR_IMU_IMU_HIPNUC_HPP

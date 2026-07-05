/*
 * imu_hipnuc.cpp
 *
 * Copyright (c) 2021-2026 Ruixiang Du (rdu)
 */

#include "sensor_imu/imu_hipnuc.hpp"

#include <cmath>

#include "async_port/async_serial.hpp"
#include "xmbase/telemetry/telemetry.hpp"

extern "C" {
#include "ch_serial.h"
}

namespace xmotion {
namespace {

constexpr float kGravity = 9.81f;               // m/s^2 per g
constexpr double kDegToRad = M_PI / 180.0;      // deg/s -> rad/s

// Map a decoded HiPNUC frame onto the HAL ImuSample. Accel is reported in
// m/s^2, gyro in rad/s; orientation is the device's unit quaternion (w,x,y,z).
hal::ImuSample ToSample(const hipnuc_raw_t &raw) {
  hal::ImuSample s;
  s.id = 0x91;

  s.accel.x = -raw.imu.acc[0] * kGravity;
  s.accel.y = -raw.imu.acc[1] * kGravity;
  s.accel.z = -raw.imu.acc[2] * kGravity;

  s.gyro.x = static_cast<float>(raw.imu.gyr[0] * kDegToRad);
  s.gyro.y = static_cast<float>(raw.imu.gyr[1] * kDegToRad);
  s.gyro.z = static_cast<float>(raw.imu.gyr[2] * kDegToRad);

  s.mag.x = raw.imu.mag[0];
  s.mag.y = raw.imu.mag[1];
  s.mag.z = raw.imu.mag[2];

  s.orientation = Quaterniond(raw.imu.quat[0], raw.imu.quat[1], raw.imu.quat[2],
                              raw.imu.quat[3]);

  s.temperature = static_cast<float>(raw.imu.temp);
  s.stamp = std::chrono::steady_clock::now();
  return s;
}

}  // namespace

ImuHipnuc::ImuHipnuc(Config cfg)
    : cfg_(std::move(cfg)), monitor_(cfg_.freshness_timeout) {
  raw_state_ = new hipnuc_raw_t{};
}

ImuHipnuc::ImuHipnuc(Config cfg, std::shared_ptr<SerialInterface> serial)
    : cfg_(std::move(cfg)),
      serial_(std::move(serial)),
      monitor_(cfg_.freshness_timeout) {
  raw_state_ = new hipnuc_raw_t{};
}

ImuHipnuc::~ImuHipnuc() {
  if (serial_) serial_->Close();
  delete static_cast<hipnuc_raw_t *>(raw_state_);
}

hal::Status ImuHipnuc::Connect() {
  if (IsConnected()) return hal::Status::kOk;

  if (!serial_) {
    serial_ = std::make_shared<AsyncSerial>(cfg_.device, cfg_.baud_rate);
  }
  if (serial_->IsOpened()) serial_->Close();

  // Reset per-connect state so a reconnect never resumes mid-frame or inherits
  // stale freshness/fault flags.
  *static_cast<hipnuc_raw_t *>(raw_state_) = hipnuc_raw_t{};
  monitor_.Reset();
  faulted_.store(false, std::memory_order_release);

  serial_->SetReceiveCallback(
      std::bind(&ImuHipnuc::OnSerialData, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));
  serial_->SetErrorCallback(
      std::bind(&ImuHipnuc::OnSerialError, this, std::placeholders::_1));

  if (!serial_->Open() || !serial_->IsOpened()) {
    XM_ERROR("ImuHipnuc: failed to open serial device {} @ {} baud",
               cfg_.device, cfg_.baud_rate);
    return hal::Status::kIoError;
  }

  XM_INFO("ImuHipnuc: connected on {} @ {} baud", cfg_.device,
            cfg_.baud_rate);
  return hal::Status::kOk;
}

void ImuHipnuc::Disconnect() {
  if (serial_ && serial_->IsOpened()) {
    serial_->Close();
    XM_INFO("ImuHipnuc: disconnected from {}", cfg_.device);
  }
  monitor_.Reset();
  faulted_.store(false, std::memory_order_release);
}

bool ImuHipnuc::IsConnected() const { return serial_ && serial_->IsOpened(); }

hal::DeviceHealth ImuHipnuc::Health() const {
  hal::DeviceHealth health = hal::HealthFromFreshness(IsConnected(), monitor_);
  // A transport link fault outranks a staleness downgrade.
  if (IsConnected() && faulted_.load(std::memory_order_acquire)) {
    return {hal::DeviceHealth::State::kFault, "transport link fault"};
  }
  return health;
}

hal::Result<hal::ImuSample> ImuHipnuc::Read() {
  if (!IsConnected()) {
    return hal::Result<hal::ImuSample>::Err(hal::Status::kNotConnected);
  }
  if (!monitor_.IsFresh()) {
    // Open but no fresh frame within the window: never hand back a stale sample.
    return hal::Result<hal::ImuSample>::Err(hal::Status::kTimeout);
  }
  std::lock_guard<std::mutex> lock(data_mtx_);
  return hal::Result<hal::ImuSample>::Ok(last_sample_);
}

void ImuHipnuc::SetSampleCallback(SampleCallback cb) {
  std::lock_guard<std::mutex> lock(data_mtx_);
  sample_cb_ = std::move(cb);
}

void ImuHipnuc::OnSerialData(std::uint8_t *data, std::size_t /*bufsize*/,
                             std::size_t len) {
  auto *raw = static_cast<hipnuc_raw_t *>(raw_state_);
  for (std::size_t i = 0; i < len; ++i) {
    const int rc = ch_serial_input(raw, data[i]);
    if (rc == 1) {
      hal::ImuSample sample = ToSample(*raw);
      monitor_.Mark();

      SampleCallback cb;
      {
        std::lock_guard<std::mutex> lock(data_mtx_);
        last_sample_ = sample;
        cb = sample_cb_;
      }
      if (cb) cb(sample);
    } else if (rc < 0) {
      // Malformed frame (bad CRC, bad length, truncated item). Never silently
      // drop it: count and log so a flaky link is observable.
      const std::uint64_t n =
          parse_error_count_.fetch_add(1, std::memory_order_relaxed) + 1;
      XM_DEBUG("ImuHipnuc: parse error on {} (total {})", cfg_.device, n);
    }
    // rc == 0: frame still accumulating; nothing to do.
  }
}

void ImuHipnuc::OnSerialError(TransportStatus status) {
  faulted_.store(true, std::memory_order_release);
  XM_ERROR("ImuHipnuc: serial link fault on {}: {}", cfg_.device,
             ToString(status));
}

}  // namespace xmotion

/*
 * imu.hpp
 *
 * Canonical HAL contract for an inertial measurement unit (ADR 0001 + 0003).
 * Replaces the legacy ImuInterface, which baked "serial + baud" into Connect()
 * and returned last-known data via a void getter with no freshness. Here:
 *   - lifecycle is the uniform Device contract (transport params at construction);
 *   - reads return Result<ImuSample> so a stale/dead sensor is kTimeout, never a
 *     stale sample dressed as a live one;
 *   - every sample carries a monotonic timestamp for downstream fusion.
 *
 * Units: accel m/s^2, gyro rad/s, mag arbitrary (device-relative), orientation
 * unit quaternion (body->world). Fields a device does not provide are zero.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

#include "xmbase/types/geometry.hpp"  // Quaterniond
#include "xmbase/types/vector.hpp"    // Vector3f (POD wire-layer vector)

#include "xmdriver/hal/device.hpp"
#include "xmdriver/hal/result.hpp"

namespace xmotion::hal {

struct ImuSample {
  std::uint32_t id = 0;
  Vector3f accel{};  // m/s^2 (members default-zero)
  Vector3f gyro{};   // rad/s
  Vector3f mag{};    // device-relative
  Quaterniond orientation = Quaterniond::Identity();  // body->world
  float temperature = 0.0f;                           // deg C (0 if unknown)
  std::chrono::steady_clock::time_point stamp{};      // host receive time
};

class Imu : public Device {
 public:
  using SampleCallback = std::function<void(const ImuSample&)>;

  // Latest sample. kNotConnected if down, kTimeout if no fresh sample within the
  // driver's freshness window, kOk with a live sample otherwise.
  virtual Result<ImuSample> Read() = 0;

  // Optional push path: invoked on the driver's RX thread for each fresh sample.
  virtual void SetSampleCallback(SampleCallback cb) = 0;
};

}  // namespace xmotion::hal

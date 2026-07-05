/*
 * health_telemetry.hpp
 *
 * The ONE place where hal::DeviceHealth::State maps onto the telemetry
 * HealthState, plus the transition-only reporting discipline every driver
 * shares. Health is reported on STATE CHANGES only (never per-sample): the
 * HAL's freshness windows already provide the hysteresis, so a driver simply
 * feeds each computed health into a HealthReporter wherever that health is
 * already evaluated (Health(), Disconnect(), a hot-unplug callback) and the
 * reporter forwards only the edges.
 *
 * Telemetry is observability only: nothing here affects control flow, takes a
 * lock, or allocates — Update() is one relaxed atomic exchange plus (on a
 * transition) the ReportHealth seam call, and it is a safe no-op when no
 * telemetry backend is bound.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <atomic>

#include "xmbase/telemetry/telemetry.hpp"
#include "xmdriver/hal/device.hpp"

namespace xmotion::hal {

// Authoritative DeviceHealth::State -> telemetry::HealthState mapping. The two
// enums are semantically 1:1 (Ok/Degraded/Fault/Disconnected); only the
// enumerator order differs, so the mapping is spelled out state by state.
constexpr telemetry::HealthState ToTelemetryHealth(DeviceHealth::State s) {
  switch (s) {
    case DeviceHealth::State::kOk:
      return telemetry::HealthState::kOk;
    case DeviceHealth::State::kDegraded:
      return telemetry::HealthState::kDegraded;
    case DeviceHealth::State::kFault:
      return telemetry::HealthState::kFault;
    case DeviceHealth::State::kDisconnected:
      return telemetry::HealthState::kDisconnected;
  }
  return telemetry::HealthState::kFault;  // unreachable; fail loud, not silent
}

// Fixed per-state detail. ReportHealth's `detail` crosses the telemetry seam
// as a pointer that must outlive the call (string-literal contract), so the
// dynamic DeviceHealth::detail text stays in the co-located XM_* events and
// health records carry these stable literals instead.
constexpr const char* HealthStateDetail(DeviceHealth::State s) {
  switch (s) {
    case DeviceHealth::State::kOk:
      return "nominal";
    case DeviceHealth::State::kDegraded:
      return "degraded";
    case DeviceHealth::State::kFault:
      return "device fault";
    case DeviceHealth::State::kDisconnected:
      return "disconnected";
  }
  return "unknown";
}

// Transition-only health reporter. Feed it the current health wherever the
// driver already computes one; ReportHealth fires only when the state differs
// from the last reported state. Thread-safe and lock-free (drivers may compute
// health from multiple threads); the initial state is kDisconnected, matching
// DeviceHealth's default, so a never-connected device reports nothing.
class HealthReporter {
 public:
  // `subsystem` must be a string literal (telemetry seam contract),
  // e.g. "driver.imu_hipnuc".
  explicit HealthReporter(const char* subsystem) noexcept
      : subsystem_(subsystem) {}

  // `detail` must be a string literal; defaults to the per-state text above.
  void Update(DeviceHealth::State state, const char* detail) noexcept {
    const DeviceHealth::State prev =
        last_.exchange(state, std::memory_order_relaxed);
    if (prev != state) {
      telemetry::ReportHealth(subsystem_, ToTelemetryHealth(state), detail);
    }
  }

  void Update(DeviceHealth::State state) noexcept {
    Update(state, HealthStateDetail(state));
  }

  void Update(const DeviceHealth& health) noexcept { Update(health.state); }

 private:
  const char* subsystem_;
  std::atomic<DeviceHealth::State> last_{DeviceHealth::State::kDisconnected};
};

}  // namespace xmotion::hal

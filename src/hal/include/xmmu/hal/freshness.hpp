/*
 * freshness.hpp
 *
 * Uniform data-freshness primitive. "Connected" is not "fresh": a device can be
 * open yet have stopped sending (unplugged mid-run, bus-off, sensor hang). The
 * old read paths returned last-known data as kOk, so a silently-dead device
 * looked healthy. FreshnessMonitor makes staleness a first-class, uniform fact:
 * a driver Mark()s every successful update; reads with no fresh sample return
 * kTimeout, and Health() degrades on staleness (ADR 0003).
 *
 * Lock-free: Mark() and the queries touch only atomics, so a hot RX/callback
 * path never blocks on a mutex.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

#include "xmmu/hal/device.hpp"

namespace xmotion::hal {

class FreshnessMonitor {
 public:
  using Clock = std::chrono::steady_clock;

  // `timeout` is how long since the last successful update before data is
  // considered stale. Pick it from the device's nominal update period with
  // margin (e.g. a 100 Hz IMU => a few missed samples, ~50 ms).
  explicit FreshnessMonitor(std::chrono::nanoseconds timeout)
      : timeout_ns_(timeout.count()) {}

  // Call on every successful device update. Thread-safe, lock-free.
  void Mark() {
    last_ns_.store(NowNs(), std::memory_order_release);
    ever_.store(true, std::memory_order_release);
  }

  // Reset to the never-updated state (e.g. on Disconnect()).
  void Reset() { ever_.store(false, std::memory_order_release); }

  bool EverUpdated() const { return ever_.load(std::memory_order_acquire); }

  std::chrono::nanoseconds Age() const {
    return std::chrono::nanoseconds(
        NowNs() - last_ns_.load(std::memory_order_acquire));
  }

  bool IsFresh() const {
    return EverUpdated() && Age().count() <= timeout_ns_;
  }

  std::chrono::nanoseconds timeout() const {
    return std::chrono::nanoseconds(timeout_ns_);
  }

 private:
  static std::int64_t NowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               Clock::now().time_since_epoch())
        .count();
  }

  const std::int64_t timeout_ns_;
  std::atomic<std::int64_t> last_ns_{0};
  std::atomic<bool> ever_{false};
};

// Compose a DeviceHealth from connection state + freshness. Drivers use this as
// the baseline and override to a fault state when the device reports one.
//   - not connected              -> kDisconnected
//   - connected, never a sample  -> kDegraded ("no data yet")
//   - connected, stale           -> kDegraded ("stale <age>")
//   - connected, fresh           -> kOk
inline DeviceHealth HealthFromFreshness(bool connected,
                                        const FreshnessMonitor& f) {
  if (!connected) return {DeviceHealth::State::kDisconnected, "not connected"};
  if (!f.EverUpdated()) return {DeviceHealth::State::kDegraded, "no data yet"};
  if (!f.IsFresh()) {
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(f.Age()).count();
    return {DeviceHealth::State::kDegraded,
            "stale " + std::to_string(ms) + "ms"};
  }
  return {DeviceHealth::State::kOk, ""};
}

}  // namespace xmotion::hal

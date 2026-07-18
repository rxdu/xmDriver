/*
 * mobile_base_driver.hpp — commander-side driver for a mobile base (spec §6-§9).
 *
 * To the upper computer the base is a single hal::Device: send a body twist plus
 * mode / e-stop, and read back status, odometry, capabilities, limits, battery,
 * and faults. Model-profile extension frames (e.g. swerve STATE_MODULE) are
 * handed to a hook verbatim so a profile layer decodes them — the core driver
 * stays model-agnostic, exactly as the protocol is.
 *
 * Thread model: AsyncCAN delivers RX frames on the shared I/O thread; the latest
 * decoded state is kept under a mutex, so Read*() is safe from any thread. Each
 * command id carries its own advancing alive counter (spec §4); "connected but
 * silent" is surfaced as kTimeout, never as stale data read back as fresh.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */

#ifndef XMOTION_MOBILE_BASE_DRIVER_HPP
#define XMOTION_MOBILE_BASE_DRIVER_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include "xmdriver/hal/device.hpp"
#include "xmdriver/hal/freshness.hpp"
#include "xmdriver/hal/result.hpp"
#include "xmdriver/transport/can_interface.hpp"

#include "mobile_base/core_codec.hpp"

namespace xmotion {

class MobileBase : public hal::Device {
 public:
  struct Config {
    std::string can_interface = "can0";
    std::uint8_t protocol_version = mobile_base::kProtocolVersion;
    // A state frame older than this reads as stale (kTimeout). Pick from the
    // base's nominal state rate with margin (e.g. 50 Hz status => ~200 ms).
    std::chrono::milliseconds state_timeout{200};
  };

  explicit MobileBase(Config config);
  ~MobileBase() override;

  MobileBase(const MobileBase&) = delete;
  MobileBase& operator=(const MobileBase&) = delete;

  // hal::Device — opens Config.can_interface and wires the RX path.
  hal::Status Connect() override;
  void Disconnect() override;
  bool IsConnected() const override;
  hal::DeviceHealth Health() const override;

  // Inject a shared or fake bus (several devices on one bus, or a test double),
  // mirroring the VESC driver. Takes ownership of `can`.
  hal::Status Connect(std::shared_ptr<CanInterface> can);

  // ---- commands (commander -> base) ---------------------------------------
  hal::Status SetTwist(double vx, double vy, double wz);
  hal::Status SetMode(mobile_base::ModeRequest mode);
  hal::Status EngageEStop();
  hal::Status ClearEStop();  // sends the clear key (spec §6.3)
  hal::Status SendHeartbeat();

  // ---- state reads (base -> commander); kTimeout if never seen or stale ----
  hal::Result<mobile_base::StatusState> ReadStatus() const;
  hal::Result<mobile_base::OdomTwistState> ReadOdomTwist() const;
  hal::Result<mobile_base::OdomPoseState> ReadOdomPose() const;
  hal::Result<mobile_base::CapabilitiesState> ReadCapabilities() const;
  hal::Result<mobile_base::LimitsState> ReadLimits() const;
  hal::Result<mobile_base::BatteryState> ReadBattery() const;
  hal::Result<mobile_base::FaultState> ReadFaults() const;

  // Model-profile extension frames (STATE_MODULE, ...) delivered verbatim for a
  // profile decoder. Fires on the I/O thread — keep the handler short and
  // non-blocking. Set it BEFORE Connect().
  using ModelFrameCallback = std::function<void(const CanFrame&)>;
  void SetModelFrameCallback(ModelFrameCallback cb) { model_cb_ = std::move(cb); }

 private:
  hal::Status Send(const CanFrame& f);  // map TransportStatus -> hal::Status
  void OnFrame(const CanFrame& f);      // RX (I/O thread): decode + snapshot

  template <typename T>
  void Store(T& slot, const T& value, hal::FreshnessMonitor& fresh) {
    std::lock_guard<std::mutex> lk(state_mtx_);
    slot = value;
    fresh.Mark();
  }
  template <typename T>
  hal::Result<T> Read(const T& slot, const hal::FreshnessMonitor& fresh) const {
    if (!connected_.load()) return hal::Result<T>::Err(hal::Status::kNotConnected);
    std::lock_guard<std::mutex> lk(state_mtx_);
    if (!fresh.IsFresh()) return hal::Result<T>::Err(hal::Status::kTimeout);
    return hal::Result<T>::Ok(slot);
  }

  Config cfg_;
  std::shared_ptr<CanInterface> can_;
  std::atomic<bool> connected_{false};

  // TX per-id alive counters (advance once per send; wrap at 256).
  std::atomic<std::uint8_t> twist_ctr_{0}, mode_ctr_{0}, estop_ctr_{0},
      hb_ctr_{0};

  // Latest decoded state, guarded by state_mtx_; freshness tracked per frame.
  mutable std::mutex state_mtx_;
  mobile_base::StatusState status_{};
  mobile_base::OdomTwistState odom_twist_{};
  mobile_base::OdomPoseState odom_pose_{};
  mobile_base::CapabilitiesState caps_{};
  mobile_base::LimitsState limits_{};
  mobile_base::BatteryState battery_{};
  mobile_base::FaultState faults_{};

  hal::FreshnessMonitor status_fresh_, odom_twist_fresh_, odom_pose_fresh_,
      caps_fresh_, limits_fresh_, battery_fresh_, faults_fresh_;

  ModelFrameCallback model_cb_;
};

}  // namespace xmotion

#endif  // XMOTION_MOBILE_BASE_DRIVER_HPP

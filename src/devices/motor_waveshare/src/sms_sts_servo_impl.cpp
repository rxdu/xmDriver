/*
 * sms_sts_servo_impl.cpp
 *
 * SmsStsServo::Impl — bridges the vendored (synchronous, half-duplex) SCServo
 * protocol to the HAL contract. Feedback is polled on a background thread into a
 * cached snapshot so the public reads never block the caller or sleep per-servo.
 * Included by sms_sts_servo.cpp (not compiled standalone).
 *
 * @copyright Copyright (c) 2024 Ruixiang Du (rdu)
 */

#include "motor_waveshare/sms_sts_servo.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <thread>

#include "SCServo.h"

#include "xmdriver/hal/freshness.hpp"
#include "xmdriver/hal/health_telemetry.hpp"
#include "xmbase/telemetry/telemetry.hpp"

namespace xmotion {
namespace {

constexpr double kDefaultMaxSpeed = 2400.0;   // step/sec envelope
constexpr double kStepsPerRev = 4095.0;       // 0..360 deg maps to 0..4095
constexpr double kMaxPositionDeg = 360.0;
constexpr double kOverTempC = 70.0;           // servo shutdown threshold margin
constexpr auto kPollPeriod = std::chrono::milliseconds(20);
constexpr auto kFreshnessTimeout = std::chrono::milliseconds(150);

constexpr double kRadToDeg = 180.0 / M_PI;
constexpr double kDegToRad = M_PI / 180.0;

double Clamp(double v, double lo, double hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

}  // namespace

class SmsStsServo::Impl {
 public:
  explicit Impl(Config cfg)
      : cfg_(std::move(cfg)), freshness_(kFreshnessTimeout) {
    if (cfg_.max_speed <= 0.0) cfg_.max_speed = kDefaultMaxSpeed;
  }

  ~Impl() { Disconnect(); }

  hal::Status Connect() {
    if (connected_.load()) return hal::Status::kOk;
    {
      std::lock_guard<std::mutex> lk(bus_mtx_);
      if (!sm_st_.begin(cfg_.baud, cfg_.bus.c_str())) {
        XM_ERROR_SRC(src_, "SmsStsServo(id={}): failed to open '{}'", cfg_.id,
                     cfg_.bus);
        return hal::Status::kIoError;
      }
    }
    freshness_.Reset();
    connected_.store(true);
    StartRefresh();
    XM_INFO_SRC(src_, "SmsStsServo(id={}): connected on '{}'", cfg_.id,
                cfg_.bus);
    return hal::Status::kOk;
  }

  void Disconnect() {
    if (!connected_.load()) return;
    Stop();  // command a safe state before tearing down
    StopRefresh();
    {
      std::lock_guard<std::mutex> lk(bus_mtx_);
      sm_st_.end();
    }
    connected_.store(false);
    freshness_.Reset();
    health_reporter_.Update(hal::DeviceHealth::State::kDisconnected);
    XM_INFO_SRC(src_, "SmsStsServo(id={}): disconnected", cfg_.id);
  }

  bool IsConnected() const { return connected_.load(); }

  hal::DeviceHealth Health() const {
    auto h = hal::HealthFromFreshness(connected_.load(), freshness_);
    if (h.state == hal::DeviceHealth::State::kOk) {
      std::lock_guard<std::mutex> lk(snap_mtx_);
      if (servo_error_ != 0) {
        h = {hal::DeviceHealth::State::kFault,
             "servo_status=0x" +
                 std::to_string(static_cast<int>(servo_error_))};
      } else if (snap_.temperature >= kOverTempC) {
        h = {hal::DeviceHealth::State::kFault,
             "over_temp " +
                 std::to_string(static_cast<int>(snap_.temperature)) + "C"};
      }
    }
    // Publish the feedback age already tracked by the freshness monitor, and
    // the health state on transitions only (both no-ops without a backend).
    if (connected_.load() && freshness_.EverUpdated()) {
      data_age_ms_.Set(
          std::chrono::duration<double, std::milli>(freshness_.Age()).count());
    }
    health_reporter_.Update(h);
    return h;
  }

  hal::Status Stop() {
    if (!connected_.load()) return hal::Status::kNotConnected;
    std::lock_guard<std::mutex> lk(bus_mtx_);
    sm_st_.WriteSpe(cfg_.id, 0, 50);  // zero speed -> safe state
    return hal::Status::kOk;
  }

  hal::Status SetPosition(hal::Radian position) {
    if (!connected_.load()) return hal::Status::kNotConnected;
    const double rad = position.value();
    if (!std::isfinite(rad)) return hal::Status::kInvalidArgument;
    const double target = rad * kRadToDeg + cfg_.position_offset_deg;
    const double clamped = Clamp(target, 0.0, kMaxPositionDeg);
    const s16 step = static_cast<s16>(clamped / kMaxPositionDeg * kStepsPerRev);
    {
      std::lock_guard<std::mutex> lk(bus_mtx_);
      sm_st_.WritePosEx(cfg_.id, step, static_cast<u16>(cfg_.max_speed), 50);
    }
    return clamped == target ? hal::Status::kOk : hal::Status::kOutOfRange;
  }

  hal::Result<hal::Radian> GetPosition() const {
    if (!connected_.load())
      return hal::Result<hal::Radian>::Err(hal::Status::kNotConnected);
    if (!freshness_.IsFresh())
      return hal::Result<hal::Radian>::Err(hal::Status::kTimeout);
    std::lock_guard<std::mutex> lk(snap_mtx_);
    return hal::Result<hal::Radian>::Ok(hal::Radian{snap_.position * kDegToRad});
  }

  hal::Status SetSpeed(hal::Rpm speed) {
    if (!connected_.load()) return hal::Status::kNotConnected;
    const double v = speed.value();  // native step/sec
    if (!std::isfinite(v)) return hal::Status::kInvalidArgument;
    const double clamped = Clamp(v, -cfg_.max_speed, cfg_.max_speed);
    {
      std::lock_guard<std::mutex> lk(bus_mtx_);
      sm_st_.WriteSpe(cfg_.id, static_cast<s16>(clamped), 50);
    }
    return clamped == v ? hal::Status::kOk : hal::Status::kOutOfRange;
  }

  hal::Result<hal::Rpm> GetSpeed() const {
    if (!connected_.load())
      return hal::Result<hal::Rpm>::Err(hal::Status::kNotConnected);
    if (!freshness_.IsFresh())
      return hal::Result<hal::Rpm>::Err(hal::Status::kTimeout);
    std::lock_guard<std::mutex> lk(snap_mtx_);
    return hal::Result<hal::Rpm>::Ok(hal::Rpm{snap_.speed});
  }

  hal::Result<State> GetState() const {
    if (!connected_.load())
      return hal::Result<State>::Err(hal::Status::kNotConnected);
    if (!freshness_.IsFresh())
      return hal::Result<State>::Err(hal::Status::kTimeout);
    std::lock_guard<std::mutex> lk(snap_mtx_);
    return hal::Result<State>::Ok(snap_);
  }

  hal::Status SetMode(Mode mode) {
    if (!connected_.load()) return hal::Status::kNotConnected;
    if (mode != Mode::kSpeed) {
      // Only wheel/speed mode is settable through the vendored wrapper.
      return hal::Status::kUnsupported;
    }
    std::lock_guard<std::mutex> lk(bus_mtx_);
    return sm_st_.WheelMode(cfg_.id) == 1 ? hal::Status::kOk
                                          : hal::Status::kIoError;
  }

  hal::Status SetMotorId(uint8_t id) {
    if (!connected_.load()) return hal::Status::kNotConnected;
    std::lock_guard<std::mutex> lk(bus_mtx_);
    sm_st_.unLockEprom(cfg_.id);
    const int ack = sm_st_.writeByte(cfg_.id, SMS_STS_ID, id);
    sm_st_.LockEprom(id);
    if (ack != 1) return hal::Status::kIoError;
    cfg_.id = id;
    return hal::Status::kOk;
  }

  hal::Status SetNeutralPosition() {
    if (!connected_.load()) return hal::Status::kNotConnected;
    std::lock_guard<std::mutex> lk(bus_mtx_);
    return sm_st_.CalibrationOfs(cfg_.id) == 1 ? hal::Status::kOk
                                               : hal::Status::kIoError;
  }

 private:
  void StartRefresh() {
    refresh_running_.store(true);
    refresh_thread_ = std::thread([this] { RefreshLoop(); });
  }

  void StopRefresh() {
    refresh_running_.store(false);
    if (refresh_thread_.joinable()) refresh_thread_.join();
  }

  // Background feedback acquisition: does the blocking half-duplex reads here,
  // off the caller's read path, and publishes a cached snapshot.
  void RefreshLoop() {
    while (refresh_running_.load()) {
      State s;
      uint8_t err = 0;
      bool ok = false;
      {
        std::lock_guard<std::mutex> lk(bus_mtx_);
        if (sm_st_.FeedBack(cfg_.id) != -1) {
          s.position = sm_st_.ReadPos(-1) / kStepsPerRev * kMaxPositionDeg -
                       cfg_.position_offset_deg;
          s.speed = sm_st_.ReadSpeed(-1);
          s.load = sm_st_.ReadLoad(-1);
          s.voltage = sm_st_.ReadVoltage(-1);
          s.temperature = sm_st_.ReadTemper(-1);
          s.is_moving = sm_st_.ReadMove(-1) != 0;
          s.current = sm_st_.ReadCurrent(-1);
          err = static_cast<uint8_t>(sm_st_.Error);
          ok = true;
        }
      }
      if (ok) {
        {
          std::lock_guard<std::mutex> lk(snap_mtx_);
          snap_ = s;
          servo_error_ = err;
        }
        freshness_.Mark();
      } else {
        poll_error_metric_.Add();  // feedback poll got no valid reply
      }
      std::this_thread::sleep_for(kPollPeriod);
    }
  }

  Config cfg_;
  SMS_STS sm_st_;
  mutable std::mutex bus_mtx_;   // serializes all half-duplex bus access
  mutable std::mutex snap_mtx_;  // guards the cached snapshot
  State snap_;
  uint8_t servo_error_ = 0;
  std::atomic<bool> connected_{false};
  hal::FreshnessMonitor freshness_;

  std::thread refresh_thread_;
  std::atomic<bool> refresh_running_{false};

  // Telemetry (observability only; handles registered once, no-op when no
  // backend is bound). Mutable members are touched from const Health().
  telemetry::EventSource src_ = telemetry::GetEventSource("driver.sms_sts");
  mutable telemetry::Gauge data_age_ms_ =
      telemetry::GetGauge("driver.sms_sts.data_age_ms");
  telemetry::Counter poll_error_metric_ =
      telemetry::GetCounter("driver.sms_sts.poll_error_count");
  mutable hal::HealthReporter health_reporter_{"driver.sms_sts"};
};

}  // namespace xmotion

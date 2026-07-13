/*
 * sms_sts_servo_array.cpp
 *
 * SmsStsServoArray + its Impl — one vendored SMS_STS protocol instance (one
 * fd, one bus mutex) shared by all configured ids, with a single background
 * poll thread that round-robins FeedBack(id) into per-id cached snapshots.
 * Mirrors the single-device SmsStsServo::Impl patterns (bus/snapshot mutexes,
 * freshness gating, health-from-freshness + servo fault overrides).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "motor_waveshare/sms_sts_servo_array.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <utility>

#include "SCServo.h"

#include "motor_waveshare/details/id_list.hpp"
#include "xmbase/telemetry/telemetry.hpp"
#include "xmdriver/hal/freshness.hpp"
#include "xmdriver/hal/health_telemetry.hpp"

namespace xmotion {
namespace {

constexpr double kDefaultMaxSpeed = 2400.0;  // step/sec envelope
constexpr double kStepsPerRev = 4095.0;      // 0..360 deg maps to 0..4095
constexpr double kMaxPositionDeg = 360.0;
constexpr double kOverTempC = 70.0;          // servo shutdown threshold margin
constexpr std::uint8_t kBroadcastId = 0xfe;  // reserved by the SCS protocol
constexpr std::uint8_t kCommandAcc = 50;     // acceleration used by commands
constexpr auto kPollPeriod = std::chrono::milliseconds(20);
// Base staleness window; one bus IO timeout per id is added on top because
// the poll round visits ids sequentially and an absent id blocks a full IO
// timeout each round (see header).
constexpr auto kFreshnessBase = std::chrono::milliseconds(150);

constexpr double kRadToDeg = 180.0 / M_PI;
constexpr double kDegToRad = M_PI / 180.0;

double Clamp(double v, double lo, double hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Severity order for aggregating member healths into the array health.
int HealthRank(hal::DeviceHealth::State s) {
  switch (s) {
    case hal::DeviceHealth::State::kOk: return 0;
    case hal::DeviceHealth::State::kDegraded: return 1;
    case hal::DeviceHealth::State::kFault: return 2;
    case hal::DeviceHealth::State::kDisconnected: return 3;
  }
  return 3;
}

}  // namespace

class SmsStsServoArray::Impl {
 public:
  explicit Impl(Config cfg) : cfg_(std::move(cfg)) {
    if (cfg_.max_speed <= 0.0) cfg_.max_speed = kDefaultMaxSpeed;
    if (cfg_.io_timeout_ms <= 0) cfg_.io_timeout_ms = 100;
    const auto freshness_timeout =
        kFreshnessBase +
        std::chrono::milliseconds(cfg_.io_timeout_ms) * cfg_.ids.size();
    channels_.reserve(cfg_.ids.size());
    for (std::uint8_t id : cfg_.ids) {
      channels_.push_back(std::make_unique<Channel>(id, freshness_timeout));
    }
  }

  ~Impl() { Disconnect(); }

  const std::vector<std::uint8_t>& ids() const { return cfg_.ids; }

  hal::Status Connect() {
    if (connected_.load()) return hal::Status::kOk;
    if (const hal::Status st = ValidateIds(); st != hal::Status::kOk) {
      XM_ERROR_SRC(src_,
                   "SmsStsServoArray: invalid id list (empty, duplicate, or "
                   "broadcast 0xfe)");
      return st;
    }
    {
      std::lock_guard<std::mutex> lk(bus_mtx_);
      sm_st_.IOTimeOut = static_cast<unsigned long>(cfg_.io_timeout_ms);
      if (!sm_st_.begin(cfg_.baud, cfg_.bus.c_str())) {
        XM_ERROR_SRC(src_, "SmsStsServoArray: failed to open '{}'", cfg_.bus);
        return hal::Status::kIoError;
      }
    }
    for (auto& ch : channels_) ch->freshness.Reset();
    connected_.store(true);
    StartRefresh();
    XM_INFO_SRC(src_, "SmsStsServoArray: connected {} servos on '{}'",
                cfg_.ids.size(), cfg_.bus);
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
    for (auto& ch : channels_) ch->freshness.Reset();
    health_reporter_.Update(hal::DeviceHealth::State::kDisconnected);
    XM_INFO_SRC(src_, "SmsStsServoArray: disconnected from '{}'", cfg_.bus);
  }

  bool IsConnected() const { return connected_.load(); }

  hal::DeviceHealth Health() const {
    hal::DeviceHealth worst{hal::DeviceHealth::State::kDisconnected,
                            "not connected"};
    if (connected_.load()) {
      worst = {hal::DeviceHealth::State::kOk, ""};
      double max_age_ms = 0.0;
      bool any_fresh = false;
      for (const auto& ch : channels_) {
        const auto h = ChannelHealth(*ch);
        if (HealthRank(h.state) > HealthRank(worst.state)) worst = h;
        if (ch->freshness.EverUpdated()) {
          any_fresh = true;
          max_age_ms = std::max(
              max_age_ms, std::chrono::duration<double, std::milli>(
                              ch->freshness.Age())
                              .count());
        }
      }
      // Publish the worst (oldest) member feedback age (no-op w/o a backend).
      if (any_fresh) data_age_ms_.Set(max_age_ms);
    }
    health_reporter_.Update(worst);
    return worst;
  }

  hal::DeviceHealth Health(std::uint8_t id) const {
    const Channel* ch = FindChannel(id);
    if (ch == nullptr) {
      return {hal::DeviceHealth::State::kDisconnected, "unknown id"};
    }
    return ChannelHealth(*ch);
  }

  hal::Status Stop() {
    if (!connected_.load()) return hal::Status::kNotConnected;
    // Failsafe fan-out: every id is commanded even if an earlier one fails.
    hal::Status aggregate = hal::Status::kOk;
    for (std::uint8_t id : cfg_.ids) {
      const hal::Status st = WriteZeroSpeed(id);
      if (st != hal::Status::kOk && aggregate == hal::Status::kOk)
        aggregate = st;
    }
    if (aggregate != hal::Status::kOk) {
      XM_WARN_SRC(src_, "SmsStsServoArray: Stop() failed on >=1 servo: {}",
                  ToString(aggregate));
    }
    return aggregate;
  }

  hal::Status Stop(std::uint8_t id) {
    if (!connected_.load()) return hal::Status::kNotConnected;
    if (FindChannel(id) == nullptr) return hal::Status::kInvalidArgument;
    return WriteZeroSpeed(id);
  }

  hal::Status SetPosition(std::uint8_t id, hal::Radian position) {
    if (!connected_.load()) return hal::Status::kNotConnected;
    if (FindChannel(id) == nullptr) return hal::Status::kInvalidArgument;
    const double rad = position.value();
    if (!std::isfinite(rad)) return hal::Status::kInvalidArgument;
    const double target = rad * kRadToDeg + cfg_.position_offset_deg;
    const double clamped = Clamp(target, 0.0, kMaxPositionDeg);
    const s16 step = static_cast<s16>(clamped / kMaxPositionDeg * kStepsPerRev);
    int ack = 0;
    {
      std::lock_guard<std::mutex> lk(bus_mtx_);
      ack = sm_st_.WritePosEx(id, step, static_cast<u16>(cfg_.max_speed),
                              kCommandAcc);
    }
    if (ack != 1) {
      command_error_metric_.Add();
      return hal::Status::kIoError;
    }
    return clamped == target ? hal::Status::kOk : hal::Status::kOutOfRange;
  }

  hal::Status SetPositions(const std::vector<hal::Radian>& positions) {
    if (!connected_.load()) return hal::Status::kNotConnected;
    if (positions.size() != cfg_.ids.size()) {
      // A partially applied command vector is unsafe: command nothing.
      XM_WARN_SRC(src_, "SmsStsServoArray: SetPositions size {} != id count {}",
                  positions.size(), cfg_.ids.size());
      return hal::Status::kInvalidArgument;
    }
    // Sync-write is one indivisible frame: validate everything up front.
    for (const hal::Radian& p : positions) {
      if (!std::isfinite(p.value())) return hal::Status::kInvalidArgument;
    }
    const std::size_t n = cfg_.ids.size();
    std::vector<u8> id_vec(cfg_.ids.begin(), cfg_.ids.end());
    std::vector<s16> pos_vec(n);
    std::vector<u16> speed_vec(n, static_cast<u16>(cfg_.max_speed));
    std::vector<u8> acc_vec(n, kCommandAcc);
    bool clamped_any = false;
    for (std::size_t i = 0; i < n; ++i) {
      const double target =
          positions[i].value() * kRadToDeg + cfg_.position_offset_deg;
      const double clamped = Clamp(target, 0.0, kMaxPositionDeg);
      if (clamped != target) clamped_any = true;
      pos_vec[i] = static_cast<s16>(clamped / kMaxPositionDeg * kStepsPerRev);
    }
    {
      std::lock_guard<std::mutex> lk(bus_mtx_);
      // Broadcast sync-write: one bus transaction, no per-servo ack.
      sm_st_.SyncWritePosEx(id_vec.data(), static_cast<u8>(n), pos_vec.data(),
                            speed_vec.data(), acc_vec.data());
    }
    return clamped_any ? hal::Status::kOutOfRange : hal::Status::kOk;
  }

  hal::Result<hal::Radian> GetPosition(std::uint8_t id) const {
    if (!connected_.load())
      return hal::Result<hal::Radian>::Err(hal::Status::kNotConnected);
    const Channel* ch = FindChannel(id);
    if (ch == nullptr)
      return hal::Result<hal::Radian>::Err(hal::Status::kInvalidArgument);
    if (!ch->freshness.IsFresh())
      return hal::Result<hal::Radian>::Err(hal::Status::kTimeout);
    std::lock_guard<std::mutex> lk(snap_mtx_);
    return hal::Result<hal::Radian>::Ok(
        hal::Radian{ch->snap.position * kDegToRad});
  }

  hal::Status SetSpeed(std::uint8_t id, hal::Rpm speed) {
    if (!connected_.load()) return hal::Status::kNotConnected;
    if (FindChannel(id) == nullptr) return hal::Status::kInvalidArgument;
    const double v = speed.value();  // native step/sec
    if (!std::isfinite(v)) return hal::Status::kInvalidArgument;
    const double clamped = Clamp(v, -cfg_.max_speed, cfg_.max_speed);
    int ack = 0;
    {
      std::lock_guard<std::mutex> lk(bus_mtx_);
      ack = sm_st_.WriteSpe(id, static_cast<s16>(clamped), kCommandAcc);
    }
    if (ack != 1) {
      command_error_metric_.Add();
      return hal::Status::kIoError;
    }
    return clamped == v ? hal::Status::kOk : hal::Status::kOutOfRange;
  }

  hal::Result<hal::Rpm> GetSpeed(std::uint8_t id) const {
    if (!connected_.load())
      return hal::Result<hal::Rpm>::Err(hal::Status::kNotConnected);
    const Channel* ch = FindChannel(id);
    if (ch == nullptr)
      return hal::Result<hal::Rpm>::Err(hal::Status::kInvalidArgument);
    if (!ch->freshness.IsFresh())
      return hal::Result<hal::Rpm>::Err(hal::Status::kTimeout);
    std::lock_guard<std::mutex> lk(snap_mtx_);
    return hal::Result<hal::Rpm>::Ok(hal::Rpm{ch->snap.speed});
  }

  hal::Result<State> GetState(std::uint8_t id) const {
    if (!connected_.load())
      return hal::Result<State>::Err(hal::Status::kNotConnected);
    const Channel* ch = FindChannel(id);
    if (ch == nullptr)
      return hal::Result<State>::Err(hal::Status::kInvalidArgument);
    if (!ch->freshness.IsFresh())
      return hal::Result<State>::Err(hal::Status::kTimeout);
    std::lock_guard<std::mutex> lk(snap_mtx_);
    return hal::Result<State>::Ok(ch->snap);
  }

  hal::Status EnableTorque(std::uint8_t id, bool enable) {
    if (!connected_.load()) return hal::Status::kNotConnected;
    if (FindChannel(id) == nullptr) return hal::Status::kInvalidArgument;
    int ack = 0;
    {
      std::lock_guard<std::mutex> lk(bus_mtx_);
      ack = sm_st_.EnableTorque(id, enable ? 1 : 0);
    }
    if (ack != 1) {
      command_error_metric_.Add();
      return hal::Status::kIoError;
    }
    return hal::Status::kOk;
  }

  hal::Status EnableTorqueAll(bool enable) {
    if (!connected_.load()) return hal::Status::kNotConnected;
    hal::Status aggregate = hal::Status::kOk;
    for (std::uint8_t id : cfg_.ids) {
      const hal::Status st = EnableTorque(id, enable);
      if (st != hal::Status::kOk && aggregate == hal::Status::kOk)
        aggregate = st;
    }
    return aggregate;
  }

 private:
  struct Channel {
    Channel(std::uint8_t servo_id, std::chrono::nanoseconds timeout)
        : id(servo_id), freshness(timeout) {}
    std::uint8_t id;
    State snap;              // guarded by snap_mtx_
    std::uint8_t error = 0;  // guarded by snap_mtx_
    hal::FreshnessMonitor freshness;
  };

  hal::Status ValidateIds() const {
    if (cfg_.ids.empty()) return hal::Status::kInvalidArgument;
    for (std::size_t i = 0; i < cfg_.ids.size(); ++i) {
      if (cfg_.ids[i] == kBroadcastId) return hal::Status::kInvalidArgument;
      for (std::size_t j = i + 1; j < cfg_.ids.size(); ++j) {
        if (cfg_.ids[i] == cfg_.ids[j]) return hal::Status::kInvalidArgument;
      }
    }
    return hal::Status::kOk;
  }

  Channel* FindChannel(std::uint8_t id) const {
    for (const auto& ch : channels_) {
      if (ch->id == id) return ch.get();
    }
    return nullptr;
  }

  hal::DeviceHealth ChannelHealth(const Channel& ch) const {
    auto h = hal::HealthFromFreshness(connected_.load(), ch.freshness);
    if (h.state == hal::DeviceHealth::State::kOk) {
      std::lock_guard<std::mutex> lk(snap_mtx_);
      if (ch.error != 0) {
        h = {hal::DeviceHealth::State::kFault,
             "id=" + std::to_string(static_cast<int>(ch.id)) +
                 " servo_status=0x" +
                 std::to_string(static_cast<int>(ch.error))};
      } else if (ch.snap.temperature >= kOverTempC) {
        h = {hal::DeviceHealth::State::kFault,
             "id=" + std::to_string(static_cast<int>(ch.id)) + " over_temp " +
                 std::to_string(static_cast<int>(ch.snap.temperature)) + "C"};
      }
    } else if (!h.detail.empty()) {
      h.detail =
          "id=" + std::to_string(static_cast<int>(ch.id)) + " " + h.detail;
    }
    return h;
  }

  hal::Status WriteZeroSpeed(std::uint8_t id) {
    int ack = 0;
    {
      std::lock_guard<std::mutex> lk(bus_mtx_);
      ack = sm_st_.WriteSpe(id, 0, kCommandAcc);  // zero speed -> safe state
    }
    if (ack != 1) {
      command_error_metric_.Add();
      return hal::Status::kIoError;
    }
    return hal::Status::kOk;
  }

  void StartRefresh() {
    refresh_running_.store(true);
    refresh_thread_ = std::thread([this] { RefreshLoop(); });
  }

  void StopRefresh() {
    refresh_running_.store(false);
    if (refresh_thread_.joinable()) refresh_thread_.join();
  }

  // Background feedback acquisition: round-robins the blocking half-duplex
  // reads over all ids here, off the caller's read path, and publishes per-id
  // cached snapshots. One round per kPollPeriod.
  void RefreshLoop() {
    while (refresh_running_.load()) {
      for (const auto& ch : channels_) {
        if (!refresh_running_.load()) return;
        State s;
        std::uint8_t err = 0;
        bool ok = false;
        {
          std::lock_guard<std::mutex> lk(bus_mtx_);
          if (sm_st_.FeedBack(ch->id) != -1) {
            s.position = sm_st_.ReadPos(-1) / kStepsPerRev * kMaxPositionDeg -
                         cfg_.position_offset_deg;
            s.speed = sm_st_.ReadSpeed(-1);
            s.load = sm_st_.ReadLoad(-1);
            s.voltage = sm_st_.ReadVoltage(-1);
            s.temperature = sm_st_.ReadTemper(-1);
            s.is_moving = sm_st_.ReadMove(-1) != 0;
            s.current = sm_st_.ReadCurrent(-1);
            err = static_cast<std::uint8_t>(sm_st_.Error);
            ok = true;
          }
        }
        if (ok) {
          {
            std::lock_guard<std::mutex> lk(snap_mtx_);
            ch->snap = s;
            ch->error = err;
          }
          ch->freshness.Mark();
        } else {
          poll_error_metric_.Add();  // feedback poll got no valid reply
        }
      }
      std::this_thread::sleep_for(kPollPeriod);
    }
  }

  Config cfg_;
  SMS_STS sm_st_;
  mutable std::mutex bus_mtx_;   // serializes all half-duplex bus access
  mutable std::mutex snap_mtx_;  // guards every Channel snapshot/error
  std::vector<std::unique_ptr<Channel>> channels_;  // fixed after ctor
  std::atomic<bool> connected_{false};

  std::thread refresh_thread_;
  std::atomic<bool> refresh_running_{false};

  // Telemetry (observability only; handles registered once, no-op when no
  // backend is bound). Names are per-class, not per-port/per-id, by the
  // low-cardinality convention. Mutable members are touched from const
  // Health().
  telemetry::EventSource src_ =
      telemetry::GetEventSource("driver.sms_sts_array");
  mutable telemetry::Gauge data_age_ms_ =
      telemetry::GetGauge("driver.sms_sts_array.data_age_ms");
  telemetry::Counter poll_error_metric_ =
      telemetry::GetCounter("driver.sms_sts_array.poll_error_count");
  // Command writes that got no valid acknowledgement from the servo.
  telemetry::Counter command_error_metric_ =
      telemetry::GetCounter("driver.sms_sts_array.command_error_count");
  mutable hal::HealthReporter health_reporter_{"driver.sms_sts_array"};
};

// --- MemberRef ---------------------------------------------------------------

hal::Status SmsStsServoArray::MemberRef::SetPosition(hal::Radian position) {
  return owner_->SetPosition(id_, position);
}
hal::Result<hal::Radian> SmsStsServoArray::MemberRef::GetPosition() {
  return owner_->GetPosition(id_);
}
hal::Status SmsStsServoArray::MemberRef::SetSpeed(hal::Rpm speed) {
  return owner_->SetSpeed(id_, speed);
}
hal::Result<hal::Rpm> SmsStsServoArray::MemberRef::GetSpeed() {
  return owner_->GetSpeed(id_);
}
hal::Status SmsStsServoArray::MemberRef::Stop() { return owner_->Stop(id_); }

// --- SmsStsServoArray --------------------------------------------------------

SmsStsServoArray::SmsStsServoArray(Config cfg) {
  members_.reserve(cfg.ids.size());
  for (std::uint8_t id : cfg.ids) {
    members_.push_back(std::unique_ptr<MemberRef>(new MemberRef(this, id)));
  }
  pimpl_ = std::make_unique<Impl>(std::move(cfg));
}

SmsStsServoArray::~SmsStsServoArray() = default;

hal::Status SmsStsServoArray::Connect() { return pimpl_->Connect(); }
void SmsStsServoArray::Disconnect() { pimpl_->Disconnect(); }
bool SmsStsServoArray::IsConnected() const { return pimpl_->IsConnected(); }
hal::DeviceHealth SmsStsServoArray::Health() const { return pimpl_->Health(); }
hal::DeviceHealth SmsStsServoArray::Health(std::uint8_t id) const {
  return pimpl_->Health(id);
}

hal::Status SmsStsServoArray::Stop() { return pimpl_->Stop(); }
hal::Status SmsStsServoArray::Stop(std::uint8_t id) { return pimpl_->Stop(id); }

hal::Status SmsStsServoArray::SetPosition(std::uint8_t id,
                                          hal::Radian position) {
  return pimpl_->SetPosition(id, position);
}
hal::Result<hal::Radian> SmsStsServoArray::GetPosition(std::uint8_t id) {
  return pimpl_->GetPosition(id);
}
hal::Status SmsStsServoArray::SetSpeed(std::uint8_t id, hal::Rpm speed) {
  return pimpl_->SetSpeed(id, speed);
}
hal::Result<hal::Rpm> SmsStsServoArray::GetSpeed(std::uint8_t id) {
  return pimpl_->GetSpeed(id);
}
hal::Result<SmsStsServoArray::State> SmsStsServoArray::GetState(
    std::uint8_t id) const {
  return pimpl_->GetState(id);
}
hal::Status SmsStsServoArray::EnableTorque(std::uint8_t id, bool enable) {
  return pimpl_->EnableTorque(id, enable);
}
hal::Status SmsStsServoArray::EnableTorqueAll(bool enable) {
  return pimpl_->EnableTorqueAll(enable);
}

hal::Status SmsStsServoArray::SetPositions(
    const std::vector<hal::Radian>& positions) {
  return pimpl_->SetPositions(positions);
}

const std::vector<std::uint8_t>& SmsStsServoArray::ids() const {
  return pimpl_->ids();
}

SmsStsServoArray::MemberRef* SmsStsServoArray::Member(std::uint8_t id) {
  for (const auto& m : members_) {
    if (m->id() == id) return m.get();
  }
  return nullptr;
}

// --- factory registration ----------------------------------------------------

bool RegisterSmsStsServoArray() {
  hal::MotorFactory::Instance().Register(
      "sms_sts_array",
      [](const hal::MotorConfig& c) -> std::unique_ptr<hal::Motor> {
        SmsStsServoArray::Config sc;
        sc.bus = c.bus;
        sc.max_speed = c.max_speed_rpm;  // native step/sec envelope
        const auto it = c.params.find("ids");
        if (it != c.params.end()) sc.ids = details::ParseIdList(it->second);
        if (sc.ids.empty()) {
          XM_ERROR("SmsStsServoArray: params[\"ids\"] missing or malformed "
                   "(expected e.g. \"1,2,3,4\")");
          return nullptr;
        }
        const auto baud = c.params.find("baud");
        if (baud != c.params.end()) {
          const long v = std::strtol(baud->second.c_str(), nullptr, 10);
          if (v <= 0) {
            XM_ERROR("SmsStsServoArray: params[\"baud\"] malformed");
            return nullptr;
          }
          sc.baud = static_cast<int>(v);
        }
        const auto off = c.params.find("position_offset_deg");
        if (off != c.params.end()) {
          char* end = nullptr;
          const double v = std::strtod(off->second.c_str(), &end);
          if (end == off->second.c_str() || !std::isfinite(v)) {
            XM_ERROR("SmsStsServoArray: params[\"position_offset_deg\"] "
                     "malformed");
            return nullptr;
          }
          sc.position_offset_deg = v;
        }
        return std::make_unique<SmsStsServoArray>(std::move(sc));
      });
  return true;
}

namespace {
// Best-effort self-registration for whole-archive linkage;
// RegisterSmsStsServoArray() is the reliable path for plain static linking.
const bool kSmsStsServoArrayRegistered = RegisterSmsStsServoArray();
}  // namespace

}  // namespace xmotion

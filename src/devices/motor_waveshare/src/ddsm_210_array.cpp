/*
 * ddsm_210_array.cpp
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "motor_waveshare/ddsm_210_array.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "async_port/async_serial.hpp"

#include "motor_waveshare/details/id_list.hpp"
#include "xmbase/telemetry/telemetry.hpp"

namespace xmotion {
namespace {

constexpr double kDefaultMaxSpeedRpm = 210.0;  // DDSM-210 mechanical limit
constexpr double kMaxPositionDeg = 360.0;
constexpr std::uint8_t kBroadcastId = 0xaa;  // reserved by the protocol

constexpr double kRadToDeg = 180.0 / M_PI;
constexpr double kDegToRad = M_PI / 180.0;

double Clamp(double v, double lo, double hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

hal::Status MapTransport(TransportStatus s) {
  switch (s) {
    case TransportStatus::kOk: return hal::Status::kOk;
    case TransportStatus::kNotOpen: return hal::Status::kNotConnected;
    case TransportStatus::kQueueFull: return hal::Status::kIoError;
    case TransportStatus::kIoError: return hal::Status::kIoError;
    case TransportStatus::kInvalidArgument: return hal::Status::kInvalidArgument;
  }
  return hal::Status::kIoError;
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

// --- MemberRef ---------------------------------------------------------------

hal::Status Ddsm210Array::MemberRef::SetSpeed(hal::Rpm speed) {
  return owner_->SetSpeed(id_, speed);
}
hal::Result<hal::Rpm> Ddsm210Array::MemberRef::GetSpeed() {
  return owner_->GetSpeed(id_);
}
hal::Status Ddsm210Array::MemberRef::SetPosition(hal::Radian position) {
  return owner_->SetPosition(id_, position);
}
hal::Result<hal::Radian> Ddsm210Array::MemberRef::GetPosition() {
  return owner_->GetPosition(id_);
}
hal::Status Ddsm210Array::MemberRef::Stop() { return owner_->Stop(id_); }

// --- Ddsm210Array ------------------------------------------------------------

Ddsm210Array::Channel::Channel(std::uint8_t motor_id,
                               std::chrono::milliseconds freshness_timeout)
    : id(motor_id), freshness(freshness_timeout) {}

Ddsm210Array::Ddsm210Array(Config cfg) : Ddsm210Array(std::move(cfg), nullptr) {}

Ddsm210Array::Ddsm210Array(Config cfg, std::shared_ptr<SerialInterface> serial)
    : cfg_(std::move(cfg)), serial_(std::move(serial)) {
  if (cfg_.max_speed_rpm <= 0.0) cfg_.max_speed_rpm = kDefaultMaxSpeedRpm;
  channels_.reserve(cfg_.ids.size());
  members_.reserve(cfg_.ids.size());
  for (std::uint8_t id : cfg_.ids) {
    channels_.push_back(std::make_unique<Channel>(id, cfg_.feedback_timeout));
    members_.push_back(
        std::unique_ptr<MemberRef>(new MemberRef(this, id)));
  }
}

Ddsm210Array::~Ddsm210Array() { Disconnect(); }

hal::Status Ddsm210Array::ValidateIds() const {
  if (cfg_.ids.empty()) return hal::Status::kInvalidArgument;
  for (std::size_t i = 0; i < cfg_.ids.size(); ++i) {
    if (cfg_.ids[i] == kBroadcastId) return hal::Status::kInvalidArgument;
    for (std::size_t j = i + 1; j < cfg_.ids.size(); ++j) {
      if (cfg_.ids[i] == cfg_.ids[j]) return hal::Status::kInvalidArgument;
    }
  }
  return hal::Status::kOk;
}

hal::Status Ddsm210Array::Connect() {
  if (connected_.load()) return hal::Status::kOk;
  if (const hal::Status st = ValidateIds(); st != hal::Status::kOk) {
    XM_ERROR_SRC(src_,
                 "Ddsm210Array: invalid id list (empty, duplicate, or "
                 "broadcast 0xaa)");
    return st;
  }
  if (!serial_) serial_ = std::make_shared<AsyncSerial>(cfg_.bus, 115200);

  serial_->SetReceiveCallback([this](uint8_t* data, size_t bufsize, size_t len) {
    ProcessRx(data, bufsize, len);
  });
  serial_->SetErrorCallback([this](TransportStatus s) {
    serial_fault_metric_.Add();
    XM_WARN_SRC(src_, "Ddsm210Array: serial fault on '{}': {}", cfg_.bus,
                ToString(s));
    connected_.store(false);
  });

  // Reset per-connect state before the port opens (the RX callback owns
  // warned_unknown_id_ once data can flow).
  warned_unknown_id_ = false;
  for (auto& ch : channels_) ch->freshness.Reset();

  if (!serial_->Open() && !serial_->IsOpened()) {
    XM_ERROR_SRC(src_, "Ddsm210Array: failed to open serial '{}'", cfg_.bus);
    return hal::Status::kIoError;
  }
  connected_.store(true);
  XM_INFO_SRC(src_, "Ddsm210Array: connected {} motors on '{}'",
              cfg_.ids.size(), cfg_.bus);
  return hal::Status::kOk;
}

void Ddsm210Array::Disconnect() {
  if (!connected_.load()) return;
  Stop();  // never leave any motor energized on the way out
  if (serial_) serial_->Close();
  connected_.store(false);
  for (auto& ch : channels_) ch->freshness.Reset();
  health_reporter_.Update(hal::DeviceHealth::State::kDisconnected);
  XM_INFO_SRC(src_, "Ddsm210Array: disconnected from '{}'", cfg_.bus);
}

bool Ddsm210Array::IsConnected() const {
  return connected_.load() && serial_ && serial_->IsOpened();
}

hal::DeviceHealth Ddsm210Array::ChannelHealth(const Channel& ch) const {
  auto h = hal::HealthFromFreshness(IsConnected(), ch.freshness);
  if (h.state == hal::DeviceHealth::State::kOk) {
    std::uint8_t err = 0;
    {
      std::lock_guard<std::mutex> lk(snap_mtx_);
      err = ch.fb.odom_feedback.error_code;
    }
    if (err != 0) {
      h = {hal::DeviceHealth::State::kFault,
           "id=" + std::to_string(static_cast<int>(ch.id)) + " error_code=0x" +
               std::to_string(static_cast<int>(err))};
    }
  } else if (!h.detail.empty()) {
    h.detail = "id=" + std::to_string(static_cast<int>(ch.id)) + " " + h.detail;
  }
  return h;
}

hal::DeviceHealth Ddsm210Array::Health() const {
  hal::DeviceHealth worst{hal::DeviceHealth::State::kDisconnected,
                          "not connected"};
  if (IsConnected()) {
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

hal::DeviceHealth Ddsm210Array::Health(std::uint8_t id) const {
  const Channel* ch = FindChannel(id);
  if (ch == nullptr) {
    return {hal::DeviceHealth::State::kDisconnected, "unknown id"};
  }
  return ChannelHealth(*ch);
}

Ddsm210Array::Channel* Ddsm210Array::FindChannel(std::uint8_t id) const {
  for (const auto& ch : channels_) {
    if (ch->id == id) return ch.get();
  }
  return nullptr;
}

Ddsm210Array::MemberRef* Ddsm210Array::Member(std::uint8_t id) {
  for (const auto& m : members_) {
    if (m->id() == id) return m.get();
  }
  return nullptr;
}

hal::Status Ddsm210Array::SendFrame(Ddsm210Frame& frame) {
  auto buffer = frame.ToBuffer();
  return MapTransport(serial_->SendBytes(buffer.data(), buffer.size()));
}

hal::Status Ddsm210Array::SendZeroSpeed(std::uint8_t id) {
  Ddsm210Frame frame(id);
  frame.SetSpeed(0.0f);  // zero speed -> the safe state
  return SendFrame(frame);
}

hal::Status Ddsm210Array::Stop() {
  if (!IsConnected()) return hal::Status::kNotConnected;
  // Failsafe fan-out: every id is commanded even if an earlier send fails.
  hal::Status aggregate = hal::Status::kOk;
  for (std::uint8_t id : cfg_.ids) {
    const hal::Status st = SendZeroSpeed(id);
    if (st != hal::Status::kOk && aggregate == hal::Status::kOk) aggregate = st;
  }
  if (aggregate != hal::Status::kOk) {
    XM_WARN_SRC(src_, "Ddsm210Array: Stop() failed on >=1 motor: {}",
                ToString(aggregate));
  }
  return aggregate;
}

hal::Status Ddsm210Array::Stop(std::uint8_t id) {
  if (!IsConnected()) return hal::Status::kNotConnected;
  if (FindChannel(id) == nullptr) return hal::Status::kInvalidArgument;
  return SendZeroSpeed(id);
}

hal::Status Ddsm210Array::SetSpeed(std::uint8_t id, hal::Rpm speed) {
  if (!IsConnected()) return hal::Status::kNotConnected;
  if (FindChannel(id) == nullptr) return hal::Status::kInvalidArgument;
  const double v = speed.value();
  if (!std::isfinite(v)) return hal::Status::kInvalidArgument;
  const double clamped = Clamp(v, -cfg_.max_speed_rpm, cfg_.max_speed_rpm);
  Ddsm210Frame frame(id);
  frame.SetSpeed(static_cast<float>(clamped));
  const hal::Status st = SendFrame(frame);
  if (st != hal::Status::kOk) return st;
  return clamped == v ? hal::Status::kOk : hal::Status::kOutOfRange;
}

hal::Status Ddsm210Array::SetSpeeds(const std::vector<hal::Rpm>& speeds) {
  if (!IsConnected()) return hal::Status::kNotConnected;
  if (speeds.size() != cfg_.ids.size()) {
    // A partially applied command vector is unsafe: command nothing.
    XM_WARN_SRC(src_, "Ddsm210Array: SetSpeeds size {} != id count {}",
                speeds.size(), cfg_.ids.size());
    return hal::Status::kInvalidArgument;
  }
  hal::Status aggregate = hal::Status::kOk;
  for (std::size_t i = 0; i < cfg_.ids.size(); ++i) {
    const hal::Status st = SetSpeed(cfg_.ids[i], speeds[i]);
    if (st != hal::Status::kOk && aggregate == hal::Status::kOk) aggregate = st;
  }
  return aggregate;
}

hal::Result<hal::Rpm> Ddsm210Array::GetSpeed(std::uint8_t id) {
  if (!IsConnected())
    return hal::Result<hal::Rpm>::Err(hal::Status::kNotConnected);
  const Channel* ch = FindChannel(id);
  if (ch == nullptr)
    return hal::Result<hal::Rpm>::Err(hal::Status::kInvalidArgument);
  if (!ch->freshness.IsFresh())
    return hal::Result<hal::Rpm>::Err(hal::Status::kTimeout);
  std::lock_guard<std::mutex> lk(snap_mtx_);
  return hal::Result<hal::Rpm>::Ok(hal::Rpm{ch->fb.speed_feedback.rpm * 0.1});
}

hal::Status Ddsm210Array::SetPosition(std::uint8_t id, hal::Radian position) {
  if (!IsConnected()) return hal::Status::kNotConnected;
  if (FindChannel(id) == nullptr) return hal::Status::kInvalidArgument;
  const double rad = position.value();
  if (!std::isfinite(rad)) return hal::Status::kInvalidArgument;
  const double deg = rad * kRadToDeg;
  const double clamped = Clamp(deg, 0.0, kMaxPositionDeg);
  Ddsm210Frame frame(id);
  frame.SetPosition(static_cast<float>(clamped));
  const hal::Status st = SendFrame(frame);
  if (st != hal::Status::kOk) return st;
  return clamped == deg ? hal::Status::kOk : hal::Status::kOutOfRange;
}

hal::Result<hal::Radian> Ddsm210Array::GetPosition(std::uint8_t id) {
  if (!IsConnected())
    return hal::Result<hal::Radian>::Err(hal::Status::kNotConnected);
  const Channel* ch = FindChannel(id);
  if (ch == nullptr)
    return hal::Result<hal::Radian>::Err(hal::Status::kInvalidArgument);
  if (!ch->freshness.IsFresh())
    return hal::Result<hal::Radian>::Err(hal::Status::kTimeout);
  std::lock_guard<std::mutex> lk(snap_mtx_);
  const double deg = ch->fb.odom_feedback.position * 360.0 / 32767.0;
  return hal::Result<hal::Radian>::Ok(hal::Radian{deg * kDegToRad});
}

hal::Result<Ddsm210Array::State> Ddsm210Array::GetState(
    std::uint8_t id) const {
  if (!IsConnected())
    return hal::Result<State>::Err(hal::Status::kNotConnected);
  const Channel* ch = FindChannel(id);
  if (ch == nullptr)
    return hal::Result<State>::Err(hal::Status::kInvalidArgument);
  if (!ch->freshness.IsFresh())
    return hal::Result<State>::Err(hal::Status::kTimeout);
  std::lock_guard<std::mutex> lk(snap_mtx_);
  State s;
  s.speed_rpm = ch->fb.speed_feedback.rpm * 0.1;
  s.position_rad = ch->fb.odom_feedback.position * 360.0 / 32767.0 * kDegToRad;
  s.encoder_count = ch->fb.odom_feedback.encoder_count;
  s.current_raw = static_cast<double>(ch->fb.speed_feedback.current);
  s.temperature_c = static_cast<double>(ch->fb.speed_feedback.temperature);
  s.error_code = ch->fb.odom_feedback.error_code;
  return hal::Result<State>::Ok(s);
}

void Ddsm210Array::RequestOdometryFeedback(std::uint8_t id) {
  if (!IsConnected() || FindChannel(id) == nullptr) return;
  Ddsm210Frame frame(id);
  frame.RequestOdom();
  SendFrame(frame);
}

void Ddsm210Array::RequestOdometryFeedbackAll() {
  for (std::uint8_t id : cfg_.ids) RequestOdometryFeedback(id);
}

void Ddsm210Array::RequestModeFeedback(std::uint8_t id) {
  if (!IsConnected() || FindChannel(id) == nullptr) return;
  Ddsm210Frame frame(id);
  frame.RequestMode();
  SendFrame(frame);
}

Ddsm210Array::Mode Ddsm210Array::GetMode(std::uint8_t id) const {
  const Channel* ch = FindChannel(id);
  if (ch == nullptr) return Mode::kUnknown;
  std::lock_guard<std::mutex> lk(snap_mtx_);
  return static_cast<Mode>(ch->fb.mode_request_feedback.mode);
}

int32_t Ddsm210Array::GetEncoderCount(std::uint8_t id) const {
  const Channel* ch = FindChannel(id);
  if (ch == nullptr) return 0;
  std::lock_guard<std::mutex> lk(snap_mtx_);
  return ch->fb.odom_feedback.encoder_count;
}

void Ddsm210Array::ProcessRx(uint8_t* data, size_t /*bufsize*/, size_t len) {
  std::vector<uint8_t> new_data(data, data + len);
  rx_buffer_.Write(new_data, new_data.size());

  // Scan for 10-byte frames; the leading byte of a valid frame carries the
  // motor id, which routes it to the matching per-id channel.
  while (rx_buffer_.GetOccupiedSize() >= 10) {
    std::vector<uint8_t> frame(10);
    rx_buffer_.Peek(frame, 10);
    Ddsm210Frame decoded(frame[0], frame);
    if (!decoded.IsValid()) {
      // Incomplete/invalid frame: discard one byte and re-synchronize.
      frame_error_metric_.Add();  // one per discarded resync byte
      rx_buffer_.Read(frame, 1);
      continue;
    }
    Channel* ch = FindChannel(frame[0]);
    if (ch == nullptr) {
      // A well-formed frame from an id we were not configured for — a
      // mis-wired bus or a stale id. Never silent, but warn only once per
      // connect to keep the RX path quiet.
      unknown_id_metric_.Add();
      if (!warned_unknown_id_) {
        warned_unknown_id_ = true;
        XM_WARN_SRC(src_, "Ddsm210Array: frame from unregistered id {}",
                    static_cast<int>(frame[0]));
      }
      rx_buffer_.Read(frame, 10);
      continue;
    }
    const auto raw = decoded.GetRawFeedback();
    {
      std::lock_guard<std::mutex> lk(snap_mtx_);
      switch (decoded.GetType()) {
        case Ddsm210Frame::Type::kSpeedFeedback:
          ch->fb.speed_feedback = raw.speed_feedback;
          break;
        case Ddsm210Frame::Type::kOdomFeedback:
          ch->fb.odom_feedback = raw.odom_feedback;
          break;
        case Ddsm210Frame::Type::kModeSwitchFeedback:
          ch->fb.mode_switch_feedback = raw.mode_switch_feedback;
          break;
        case Ddsm210Frame::Type::kModeRequestFeedback:
          ch->fb.mode_request_feedback = raw.mode_request_feedback;
          break;
        default:
          break;
      }
    }
    ch->freshness.Mark();  // a valid frame is a fresh sample for this id
    rx_buffer_.Read(frame, 10);
  }
}

// --- factory registration ----------------------------------------------------

bool RegisterDdsm210Array() {
  hal::MotorFactory::Instance().Register(
      "ddsm210_array",
      [](const hal::MotorConfig& c) -> std::unique_ptr<hal::Motor> {
        Ddsm210Array::Config ac;
        ac.bus = c.bus;
        ac.max_speed_rpm = c.max_speed_rpm;
        const auto it = c.params.find("ids");
        if (it != c.params.end()) ac.ids = details::ParseIdList(it->second);
        if (ac.ids.empty()) {
          XM_ERROR("Ddsm210Array: params[\"ids\"] missing or malformed "
                   "(expected e.g. \"1,2,3,4\")");
          return nullptr;
        }
        return std::make_unique<Ddsm210Array>(std::move(ac));
      });
  return true;
}

namespace {
// Best-effort self-registration for whole-archive linkage;
// RegisterDdsm210Array() is the reliable path for plain static linking.
const bool kDdsm210ArrayRegistered = RegisterDdsm210Array();
}  // namespace

}  // namespace xmotion

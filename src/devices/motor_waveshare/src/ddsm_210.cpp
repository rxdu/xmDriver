/*
 * ddsm_210.cpp
 *
 * @copyright Copyright (c) 2024 Ruixiang Du (rdu)
 */

#include "motor_waveshare/ddsm_210.hpp"

#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

#include "async_port/async_serial.hpp"

#include "xmbase/telemetry/telemetry.hpp"

namespace xmotion {
namespace {

constexpr double kDefaultMaxSpeedRpm = 210.0;  // DDSM-210 mechanical limit
constexpr double kMaxPositionDeg = 360.0;
// Feedback arrives on every command reply; a few missed replies => stale.
constexpr auto kFreshnessTimeout = std::chrono::milliseconds(200);

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

}  // namespace

Ddsm210::Ddsm210(Config cfg) : Ddsm210(std::move(cfg), nullptr) {}

Ddsm210::Ddsm210(Config cfg, std::shared_ptr<SerialInterface> serial)
    : cfg_(std::move(cfg)),
      serial_(std::move(serial)),
      freshness_(kFreshnessTimeout) {
  if (cfg_.max_speed_rpm <= 0.0) cfg_.max_speed_rpm = kDefaultMaxSpeedRpm;
}

Ddsm210::~Ddsm210() { Disconnect(); }

hal::Status Ddsm210::Connect() {
  if (connected_) return hal::Status::kOk;
  if (!serial_) serial_ = std::make_shared<AsyncSerial>(cfg_.bus, 115200);

  serial_->SetReceiveCallback([this](uint8_t* data, size_t bufsize, size_t len) {
    ProcessFeedback(data, bufsize, len);
  });
  serial_->SetErrorCallback([this](TransportStatus s) {
    XM_WARN("Ddsm210(id={}): serial fault: {}", cfg_.id, ToString(s));
    connected_.store(false);
  });

  if (!serial_->Open() && !serial_->IsOpened()) {
    XM_ERROR("Ddsm210(id={}): failed to open serial '{}'", cfg_.id, cfg_.bus);
    return hal::Status::kIoError;
  }
  freshness_.Reset();
  connected_.store(true);
  XM_INFO("Ddsm210(id={}): connected on '{}'", cfg_.id, cfg_.bus);
  return hal::Status::kOk;
}

void Ddsm210::Disconnect() {
  if (!connected_.load()) return;
  Stop();  // never leave the motor energized on the way out
  if (serial_) serial_->Close();
  connected_.store(false);
  freshness_.Reset();
  XM_INFO("Ddsm210(id={}): disconnected", cfg_.id);
}

bool Ddsm210::IsConnected() const {
  return connected_.load() && serial_ && serial_->IsOpened();
}

hal::DeviceHealth Ddsm210::Health() const {
  auto h = hal::HealthFromFreshness(IsConnected(), freshness_);
  const uint8_t err = raw_feedback_.odom_feedback.error_code;
  if (h.state == hal::DeviceHealth::State::kOk && err != 0) {
    return {hal::DeviceHealth::State::kFault,
            "error_code=0x" + std::to_string(static_cast<int>(err))};
  }
  return h;
}

hal::Status Ddsm210::SendFrame(Ddsm210Frame& frame) {
  auto buffer = frame.ToBuffer();
  return MapTransport(serial_->SendBytes(buffer.data(), buffer.size()));
}

hal::Status Ddsm210::Stop() {
  if (!IsConnected()) return hal::Status::kNotConnected;
  Ddsm210Frame frame(cfg_.id);
  frame.SetSpeed(0.0f);  // zero speed -> the safe state
  return SendFrame(frame);
}

hal::Status Ddsm210::SetSpeed(hal::Rpm speed) {
  if (!IsConnected()) return hal::Status::kNotConnected;
  const double v = speed.value();
  if (!std::isfinite(v)) return hal::Status::kInvalidArgument;
  const double clamped = Clamp(v, -cfg_.max_speed_rpm, cfg_.max_speed_rpm);
  Ddsm210Frame frame(cfg_.id);
  frame.SetSpeed(static_cast<float>(clamped));
  const hal::Status st = SendFrame(frame);
  if (st != hal::Status::kOk) return st;
  return clamped == v ? hal::Status::kOk : hal::Status::kOutOfRange;
}

hal::Result<hal::Rpm> Ddsm210::GetSpeed() {
  if (!IsConnected())
    return hal::Result<hal::Rpm>::Err(hal::Status::kNotConnected);
  if (!freshness_.IsFresh())
    return hal::Result<hal::Rpm>::Err(hal::Status::kTimeout);
  return hal::Result<hal::Rpm>::Ok(
      hal::Rpm{raw_feedback_.speed_feedback.rpm * 0.1});
}

hal::Status Ddsm210::SetPosition(hal::Radian position) {
  if (!IsConnected()) return hal::Status::kNotConnected;
  const double rad = position.value();
  if (!std::isfinite(rad)) return hal::Status::kInvalidArgument;
  const double deg = rad * kRadToDeg;
  const double clamped = Clamp(deg, 0.0, kMaxPositionDeg);
  Ddsm210Frame frame(cfg_.id);
  frame.SetPosition(static_cast<float>(clamped));
  const hal::Status st = SendFrame(frame);
  if (st != hal::Status::kOk) return st;
  return clamped == deg ? hal::Status::kOk : hal::Status::kOutOfRange;
}

hal::Result<hal::Radian> Ddsm210::GetPosition() {
  if (!IsConnected())
    return hal::Result<hal::Radian>::Err(hal::Status::kNotConnected);
  if (!freshness_.IsFresh())
    return hal::Result<hal::Radian>::Err(hal::Status::kTimeout);
  const double deg = raw_feedback_.odom_feedback.position * 360.0 / 32767.0;
  return hal::Result<hal::Radian>::Ok(hal::Radian{deg * kDegToRad});
}

void Ddsm210::RequestOdometryFeedback() {
  if (!IsConnected()) return;
  Ddsm210Frame frame(cfg_.id);
  frame.RequestOdom();
  SendFrame(frame);
}

void Ddsm210::RequestModeFeedback() {
  if (!IsConnected()) return;
  Ddsm210Frame frame(cfg_.id);
  frame.RequestMode();
  SendFrame(frame);
}

Ddsm210::Mode Ddsm210::GetMode() const {
  return static_cast<Mode>(raw_feedback_.mode_request_feedback.mode);
}

int32_t Ddsm210::GetEncoderCount() const {
  return raw_feedback_.odom_feedback.encoder_count;
}

hal::Status Ddsm210::SetMode(Mode mode, uint32_t timeout_ms) {
  if (mode != Mode::kOpenLoop && mode != Mode::kSpeed &&
      mode != Mode::kPosition) {
    XM_WARN("Ddsm210::SetMode: invalid mode");
    return hal::Status::kInvalidArgument;
  }
  if (!IsConnected()) return hal::Status::kNotConnected;

  Ddsm210Frame frame(cfg_.id);
  frame.SetMode(mode);
  if (const hal::Status st = SendFrame(frame); st != hal::Status::kOk)
    return st;

  // Config-time handshake: bounded blocking poll for confirmation.
  const auto step = std::chrono::milliseconds(std::max<uint32_t>(timeout_ms / 10, 1));
  for (int i = 0; i < 10; ++i) {
    RequestModeFeedback();
    std::this_thread::sleep_for(step);
    if (GetMode() == mode) return hal::Status::kOk;
  }
  XM_WARN("Ddsm210::SetMode: mode not confirmed within timeout");
  return hal::Status::kTimeout;
}

hal::Status Ddsm210::SetMotorId(uint8_t id, uint32_t timeout_ms) {
  if (id == 0xaa) {
    XM_WARN("Ddsm210::SetMotorId: 0xaa is reserved for broadcast");
    return hal::Status::kInvalidArgument;
  }
  if (!IsConnected()) return hal::Status::kNotConnected;

  id_set_ack_received_.store(false);
  cfg_.id = id;
  // Config-time handshake: bounded blocking. The device wants the set-id frame
  // repeated a few times, then acknowledges on the 0x64 reply.
  for (int i = 0; i < 5; ++i) {
    Ddsm210Frame frame(cfg_.id);
    frame.SetId(id);
    if (const hal::Status st = SendFrame(frame); st != hal::Status::kOk)
      return st;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  const auto step = std::chrono::milliseconds(std::max<uint32_t>(timeout_ms / 10, 1));
  for (int i = 0; i < 10; ++i) {
    if (id_set_ack_received_.load()) return hal::Status::kOk;
    std::this_thread::sleep_for(step);
  }
  return hal::Status::kTimeout;
}

void Ddsm210::ProcessFeedback(uint8_t* data, size_t /*bufsize*/, size_t len) {
  std::vector<uint8_t> new_data(data, data + len);
  rx_buffer_.Write(new_data, new_data.size());

  // only try to decode when there are at least 10 bytes in the buffer
  while (rx_buffer_.GetOccupiedSize() >= 10) {
    std::vector<uint8_t> frame(10);
    rx_buffer_.Peek(frame, 10);
    Ddsm210Frame ddsm_frame(cfg_.id, frame);
    if (ddsm_frame.IsValid()) {
      auto new_raw_feedback = ddsm_frame.GetRawFeedback();
      switch (ddsm_frame.GetType()) {
        case Ddsm210Frame::Type::kSpeedFeedback:
          raw_feedback_.speed_feedback = new_raw_feedback.speed_feedback;
          // note: motor id ack shares the same 0x64 frame id
          id_set_ack_received_.store(true);
          break;
        case Ddsm210Frame::Type::kOdomFeedback:
          raw_feedback_.odom_feedback = new_raw_feedback.odom_feedback;
          break;
        case Ddsm210Frame::Type::kModeSwitchFeedback:
          raw_feedback_.mode_switch_feedback =
              new_raw_feedback.mode_switch_feedback;
          break;
        case Ddsm210Frame::Type::kModeRequestFeedback:
          raw_feedback_.mode_request_feedback =
              new_raw_feedback.mode_request_feedback;
          break;
        default:
          break;
      }
      freshness_.Mark();  // a valid frame is a fresh sample
      rx_buffer_.Read(frame, 10);
    } else {
      // incomplete/invalid frame, discard the first byte and try again
      rx_buffer_.Read(frame, 1);
    }
  }
}

bool RegisterDdsm210Motor() {
  hal::MotorFactory::Instance().Register(
      "ddsm210", [](const hal::MotorConfig& c) -> std::unique_ptr<hal::Motor> {
        Ddsm210::Config dc;
        dc.bus = c.bus;
        dc.id = static_cast<std::uint8_t>(c.id);
        dc.max_speed_rpm = c.max_speed_rpm;
        return std::make_unique<Ddsm210>(std::move(dc));
      });
  return true;
}

namespace {
// Best-effort self-registration for whole-archive linkage; RegisterDdsm210Motor()
// is the reliable path for plain static linking.
const bool kDdsm210Registered = RegisterDdsm210Motor();
}  // namespace

}  // namespace xmotion

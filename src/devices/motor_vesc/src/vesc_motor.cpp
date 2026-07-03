/*
 * vesc_motor.cpp
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "motor_vesc/vesc_motor.hpp"

#include <chrono>
#include <cmath>

#include "xmbase/logging/xlogger.hpp"

namespace xmotion {
namespace {

constexpr double kDefaultMaxSpeedErpm = 30000.0;
constexpr double kDefaultMaxCurrentA = 20.0;
constexpr double kDefaultFreshnessMs = 200.0;

double Clamp(double v, double lo, double hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

// Map a transport outcome onto the HAL vocabulary so a failed send surfaces as
// a real status instead of being swallowed.
hal::Status FromTransport(TransportStatus st) {
  switch (st) {
    case TransportStatus::kOk: return hal::Status::kOk;
    case TransportStatus::kNotOpen: return hal::Status::kNotConnected;
    case TransportStatus::kInvalidArgument: return hal::Status::kInvalidArgument;
    case TransportStatus::kQueueFull:
    case TransportStatus::kIoError: return hal::Status::kIoError;
  }
  return hal::Status::kIoError;
}

std::chrono::nanoseconds FreshnessWindow(double ms) {
  const double v = ms > 0.0 ? ms : kDefaultFreshnessMs;
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::duration<double, std::milli>(v));
}

const char* FaultToString(int32_t code) {
  switch (code) {
    case kFaultCodeNone: return "";
    case kFaultCodeOverVoltage: return "over_voltage";
    case kFaultCodeUnderVoltage: return "under_voltage";
    case kFaultCodeDrv8302: return "drv8302";
    case kFaultCodeAbsOverCurrent: return "abs_over_current";
    case kFaultCodeOverTempFET: return "over_temp_fet";
    case kFaultCodeOverTempMotor: return "over_temp_motor";
    default: return "unknown_fault";
  }
}

}  // namespace

VescMotor::VescMotor(Config cfg) : VescMotor(std::move(cfg), nullptr) {}

VescMotor::VescMotor(Config cfg, std::shared_ptr<CanInterface> can)
    : cfg_(std::move(cfg)),
      injected_can_(std::move(can)),
      monitor_(FreshnessWindow(cfg_.freshness_ms)) {
  if (cfg_.max_speed_erpm <= 0.0) cfg_.max_speed_erpm = kDefaultMaxSpeedErpm;
  if (cfg_.max_current_a <= 0.0) cfg_.max_current_a = kDefaultMaxCurrentA;
  if (cfg_.freshness_ms <= 0.0) cfg_.freshness_ms = kDefaultFreshnessMs;
}

hal::Status VescMotor::Connect() {
  if (connected_) return hal::Status::kOk;
  monitor_.Reset();  // no fresh feedback until the first status frame arrives
  bus_fault_.store(false);
  // Mark freshness on every VESC status frame (runs on the I/O thread).
  vesc_.SetStateUpdatedCallback(
      [this](const StampedVescState&) { monitor_.Mark(); });
  // An async bus fault (bus-off, interface down, unplug) latches so Health()
  // reports kFault instead of a link that only looks stale.
  vesc_.SetErrorCallback([this](TransportStatus reason) {
    XLOG_ERROR("VescMotor: CAN bus fault on vesc {}: {}",
               static_cast<int>(cfg_.id), ToString(reason));
    bus_fault_.store(true);
  });
  const bool opened = injected_can_ ? vesc_.Connect(injected_can_, cfg_.id)
                                    : vesc_.Connect(cfg_.bus, cfg_.id);
  if (!opened) {
    XLOG_ERROR("VescMotor: failed to open CAN bus '{}' for vesc {}", cfg_.bus,
               static_cast<int>(cfg_.id));
    return hal::Status::kIoError;
  }
  connected_ = true;
  XLOG_INFO("VescMotor: connected on '{}' vesc {}", cfg_.bus,
            static_cast<int>(cfg_.id));
  return hal::Status::kOk;
}

void VescMotor::Disconnect() {
  if (connected_) {
    Stop();  // never leave the motor energized on the way out
    vesc_.Disconnect();
    monitor_.Reset();
    bus_fault_.store(false);
    connected_ = false;
    XLOG_INFO("VescMotor: disconnected vesc {}", static_cast<int>(cfg_.id));
  }
}

bool VescMotor::IsConnected() const { return connected_; }

hal::DeviceHealth VescMotor::Health() const {
  // Freshness is the baseline: a connected-but-silent VESC degrades instead of
  // looking nominal. A reported fault overrides to kFault.
  hal::DeviceHealth health = hal::HealthFromFreshness(connected_, monitor_);
  if (connected_) {
    // A latched async bus fault overrides freshness: the link is known bad.
    if (bus_fault_.load())
      return {hal::DeviceHealth::State::kFault, "can bus fault"};
    const int32_t fault = vesc_.GetLastState().state.fault_code;
    if (fault != kFaultCodeNone)
      return {hal::DeviceHealth::State::kFault, FaultToString(fault)};
  }
  return health;
}

hal::Status VescMotor::Stop() {
  if (!connected_) return hal::Status::kNotConnected;
  return FromTransport(vesc_.SetCurrent(0.0));  // zero torque -> coast
}

hal::Status VescMotor::SetSpeed(hal::Rpm speed) {
  if (!connected_) return hal::Status::kNotConnected;
  const double v = speed.value();
  if (!std::isfinite(v)) return hal::Status::kInvalidArgument;
  const double clamped = Clamp(v, -cfg_.max_speed_erpm, cfg_.max_speed_erpm);
  const hal::Status sent = FromTransport(vesc_.SetSpeed(clamped));
  if (sent != hal::Status::kOk) return sent;  // don't hide a failed send
  return clamped == v ? hal::Status::kOk : hal::Status::kOutOfRange;
}

hal::Result<hal::Rpm> VescMotor::GetSpeed() {
  if (!connected_) return hal::Result<hal::Rpm>::Err(hal::Status::kNotConnected);
  // A silently-dead-but-connected VESC must not report stale values as kOk.
  if (!monitor_.IsFresh())
    return hal::Result<hal::Rpm>::Err(hal::Status::kTimeout);
  return hal::Result<hal::Rpm>::Ok(hal::Rpm{vesc_.GetLastState().state.speed});
}

hal::Status VescMotor::SetCurrent(hal::Ampere current) {
  if (!connected_) return hal::Status::kNotConnected;
  const double a = current.value();
  if (!std::isfinite(a)) return hal::Status::kInvalidArgument;
  const double clamped = Clamp(a, -cfg_.max_current_a, cfg_.max_current_a);
  const hal::Status sent = FromTransport(vesc_.SetCurrent(clamped));
  if (sent != hal::Status::kOk) return sent;
  return clamped == a ? hal::Status::kOk : hal::Status::kOutOfRange;
}

hal::Result<hal::Ampere> VescMotor::GetCurrent() {
  if (!connected_)
    return hal::Result<hal::Ampere>::Err(hal::Status::kNotConnected);
  if (!monitor_.IsFresh())
    return hal::Result<hal::Ampere>::Err(hal::Status::kTimeout);
  return hal::Result<hal::Ampere>::Ok(
      hal::Ampere{vesc_.GetLastState().state.current_motor});
}

hal::Status VescMotor::ApplyBrake(hal::Ratio amount) {
  if (!connected_) return hal::Status::kNotConnected;
  const double r = amount.value();
  if (!std::isfinite(r) || r < 0.0 || r > 1.0) return hal::Status::kOutOfRange;
  // ratio -> brake current (A)
  return FromTransport(vesc_.SetBrake(r * cfg_.max_current_a));
}

hal::Status VescMotor::ReleaseBrake() {
  if (!connected_) return hal::Status::kNotConnected;
  return FromTransport(vesc_.SetCurrent(0.0));  // release the brake into coast
}

bool RegisterVescMotor() {
  hal::MotorFactory::Instance().Register(
      "vesc", [](const hal::MotorConfig& c) -> std::unique_ptr<hal::Motor> {
        VescMotor::Config vc;
        vc.bus = c.bus;
        vc.id = static_cast<std::uint8_t>(c.id);
        vc.max_speed_erpm = c.max_speed_rpm;
        vc.max_current_a = c.max_current_a;
        return std::make_unique<VescMotor>(std::move(vc));
      });
  return true;
}

namespace {
// Self-registration for whole-archive / header-referencing linkage. The explicit
// RegisterVescMotor() above is the reliable path for plain static linking.
const bool kVescMotorRegistered = RegisterVescMotor();
}  // namespace

}  // namespace xmotion

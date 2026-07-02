/*
 * vesc_motor.cpp
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "motor_vesc/vesc_motor.hpp"

#include <cmath>

namespace xmotion {
namespace {

constexpr double kDefaultMaxSpeedErpm = 30000.0;
constexpr double kDefaultMaxCurrentA = 20.0;

double Clamp(double v, double lo, double hi) {
  return v < lo ? lo : (v > hi ? hi : v);
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

VescMotor::VescMotor(Config cfg) : cfg_(std::move(cfg)) {
  if (cfg_.max_speed_erpm <= 0.0) cfg_.max_speed_erpm = kDefaultMaxSpeedErpm;
  if (cfg_.max_current_a <= 0.0) cfg_.max_current_a = kDefaultMaxCurrentA;
}

hal::Status VescMotor::Connect() {
  if (connected_) return hal::Status::kOk;
  if (!vesc_.Connect(cfg_.bus, cfg_.id)) return hal::Status::kIoError;
  connected_ = true;
  return hal::Status::kOk;
}

void VescMotor::Disconnect() {
  if (connected_) {
    Stop();  // never leave the motor energized on the way out
    vesc_.Disconnect();
    connected_ = false;
  }
}

bool VescMotor::IsConnected() const { return connected_; }

hal::DeviceHealth VescMotor::Health() const {
  if (!connected_) return {hal::DeviceHealth::State::kDisconnected, ""};
  const int32_t fault = vesc_.GetLastState().state.fault_code;
  if (fault != kFaultCodeNone)
    return {hal::DeviceHealth::State::kFault, FaultToString(fault)};
  return {hal::DeviceHealth::State::kOk, ""};
}

hal::Status VescMotor::Stop() {
  if (!connected_) return hal::Status::kNotConnected;
  vesc_.SetCurrent(0.0);  // zero torque -> coast; the safe state
  return hal::Status::kOk;
}

hal::Status VescMotor::SetSpeed(hal::Rpm speed) {
  if (!connected_) return hal::Status::kNotConnected;
  const double v = speed.value();
  if (!std::isfinite(v)) return hal::Status::kInvalidArgument;
  const double clamped = Clamp(v, -cfg_.max_speed_erpm, cfg_.max_speed_erpm);
  vesc_.SetSpeed(clamped);
  return clamped == v ? hal::Status::kOk : hal::Status::kOutOfRange;
}

hal::Result<hal::Rpm> VescMotor::GetSpeed() {
  if (!connected_) return hal::Result<hal::Rpm>::Err(hal::Status::kNotConnected);
  return hal::Result<hal::Rpm>::Ok(hal::Rpm{vesc_.GetLastState().state.speed});
}

hal::Status VescMotor::SetCurrent(hal::Ampere current) {
  if (!connected_) return hal::Status::kNotConnected;
  const double a = current.value();
  if (!std::isfinite(a)) return hal::Status::kInvalidArgument;
  const double clamped = Clamp(a, -cfg_.max_current_a, cfg_.max_current_a);
  vesc_.SetCurrent(clamped);
  return clamped == a ? hal::Status::kOk : hal::Status::kOutOfRange;
}

hal::Result<hal::Ampere> VescMotor::GetCurrent() {
  if (!connected_)
    return hal::Result<hal::Ampere>::Err(hal::Status::kNotConnected);
  return hal::Result<hal::Ampere>::Ok(
      hal::Ampere{vesc_.GetLastState().state.current_motor});
}

hal::Status VescMotor::ApplyBrake(hal::Ratio amount) {
  if (!connected_) return hal::Status::kNotConnected;
  const double r = amount.value();
  if (!std::isfinite(r) || r < 0.0 || r > 1.0) return hal::Status::kOutOfRange;
  vesc_.SetBrake(r * cfg_.max_current_a);  // ratio -> brake current (A)
  return hal::Status::kOk;
}

hal::Status VescMotor::ReleaseBrake() {
  if (!connected_) return hal::Status::kNotConnected;
  vesc_.SetCurrent(0.0);  // release the brake into coast
  return hal::Status::kOk;
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

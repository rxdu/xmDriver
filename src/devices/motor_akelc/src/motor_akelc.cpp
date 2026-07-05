/*
 * motor_akelc.cpp
 *
 * Created on 4/20/24 11:44 PM
 * Description: AKELC driver on the xmotion::hal Motor contract.
 *
 * Copyright (c) 2024 Ruixiang Du (rdu)
 */

#include "motor_akelc/motor_akelc.hpp"

#include <cmath>
#include <limits>
#include <utility>

#include "modbus_rtu/modbus_rtu_port.hpp"
#include "xmbase/telemetry/telemetry.hpp"

namespace xmotion {
namespace {

// The target-speed register is 16-bit signed, so this is the widest sane
// default envelope when the config leaves max_speed_rpm unset.
constexpr double kDefaultMaxSpeedRpm =
    static_cast<double>(std::numeric_limits<int16_t>::max());

double Clamp(double v, double lo, double hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

const char* ErrorToString(MotorAkelcModbus::ErrorCode code) {
  using E = MotorAkelcModbus::ErrorCode;
  switch (code) {
    case E::kNoError: return "";
    case E::kBlocked: return "blocked";
    case E::kUnableToReachTargetRpm: return "unable_to_reach_target_rpm";
    case E::kOverCurrentStop: return "over_current_stop";
    case E::kOverHeatStop: return "over_heat_stop";
    case E::kOverVoltageStop: return "over_voltage_stop";
    case E::kUnderVoltageStop: return "under_voltage_stop";
    case E::kCircuitShortStop: return "circuit_short_stop";
    case E::kBrakingCurrentException: return "braking_current_exception";
    default: return "unknown_fault";
  }
}

}  // namespace

MotorAkelc::MotorAkelc(Config cfg) : cfg_(std::move(cfg)) {
  if (cfg_.max_speed_rpm <= 0.0) cfg_.max_speed_rpm = kDefaultMaxSpeedRpm;
}

MotorAkelc::MotorAkelc(Config cfg, std::shared_ptr<ModbusRtuInterface> port)
    : cfg_(std::move(cfg)), port_(std::move(port)) {
  if (cfg_.max_speed_rpm <= 0.0) cfg_.max_speed_rpm = kDefaultMaxSpeedRpm;
}

MotorAkelc::~MotorAkelc() { Disconnect(); }

hal::Status MotorAkelc::Connect() {
  if (connected_) return hal::Status::kOk;

  if (!port_) port_ = std::make_shared<ModbusRtuPort>();

  if (!port_->IsOpened()) {
    if (!port_->Open(cfg_.bus, cfg_.baud_rate, ModbusRtuInterface::Parity::kEven,
                     ModbusRtuInterface::DataBit::kBit8,
                     ModbusRtuInterface::StopBit::kBit1)) {
      XM_ERROR("[Akelc] Failed to open Modbus port {} @ {} baud", cfg_.bus,
                 cfg_.baud_rate);
      return hal::Status::kIoError;
    }
  }

  impl_ = std::make_unique<MotorAkelcModbus>(port_, cfg_.device_id);

  // Confirm the device actually answers before declaring ourselves online — a
  // dead bus must not look connected.
  if (!impl_->IsReachable()) {
    XM_ERROR("[Akelc] Device id {} on {} did not respond", cfg_.device_id,
               cfg_.bus);
    impl_.reset();
    return hal::Status::kTimeout;
  }

  connected_ = true;
  XM_INFO("[Akelc] Connected to device id {} on {}", cfg_.device_id,
            cfg_.bus);
  return hal::Status::kOk;
}

void MotorAkelc::Disconnect() {
  if (!connected_) return;
  Stop();  // never leave the motor commanded on the way out
  if (port_) port_->Close();
  impl_.reset();
  connected_ = false;
  XM_INFO("[Akelc] Disconnected device id {} on {}", cfg_.device_id,
            cfg_.bus);
}

bool MotorAkelc::IsConnected() const { return connected_; }

hal::DeviceHealth MotorAkelc::Health() const {
  if (!connected_ || !impl_)
    return {hal::DeviceHealth::State::kDisconnected, "not connected"};

  // Reading the fault/blocked registers also Mark()s freshness on success, so
  // compute the freshness baseline afterwards.
  auto err = impl_->GetErrorCode();
  auto blocked = impl_->IsMotorBlocked();

  if (err.ok() && err.value != MotorAkelcModbus::ErrorCode::kNoError)
    return {hal::DeviceHealth::State::kFault, ErrorToString(err.value)};
  if (blocked.ok() && blocked.value)
    return {hal::DeviceHealth::State::kFault, "motor blocked"};

  return hal::HealthFromFreshness(true, impl_->freshness());
}

hal::Status MotorAkelc::Stop() {
  if (!connected_ || !impl_) return hal::Status::kNotConnected;
  return impl_->SetTargetRpm(0);  // zero output -> coast, the safe state
}

hal::Status MotorAkelc::SetSpeed(hal::Rpm speed) {
  if (!connected_ || !impl_) return hal::Status::kNotConnected;
  const double v = speed.value();
  if (!std::isfinite(v)) return hal::Status::kInvalidArgument;

  const double clamped = Clamp(v, -cfg_.max_speed_rpm, cfg_.max_speed_rpm);
  const hal::Status s =
      impl_->SetTargetRpm(static_cast<int32_t>(std::llround(clamped)));
  if (s != hal::Status::kOk) return s;  // surface the transport error
  return clamped == v ? hal::Status::kOk : hal::Status::kOutOfRange;
}

hal::Result<hal::Rpm> MotorAkelc::GetSpeed() {
  if (!connected_ || !impl_)
    return hal::Result<hal::Rpm>::Err(hal::Status::kNotConnected);
  auto r = impl_->GetActualRpm();
  if (!r.ok()) return hal::Result<hal::Rpm>::Err(r.status);
  return hal::Result<hal::Rpm>::Ok(hal::Rpm{static_cast<double>(r.value)});
}

hal::Status MotorAkelc::ApplyBrake(hal::Ratio amount) {
  if (!connected_ || !impl_) return hal::Status::kNotConnected;
  const double r = amount.value();
  if (!std::isfinite(r)) return hal::Status::kInvalidArgument;
  if (r < 0.0 || r > 1.0) return hal::Status::kOutOfRange;
  return impl_->ApplyBrake(static_cast<float>(r * 100.0));  // ratio -> percent
}

hal::Status MotorAkelc::ReleaseBrake() {
  if (!connected_ || !impl_) return hal::Status::kNotConnected;
  return impl_->ReleaseBrake();
}

bool RegisterAkelcMotor() {
  hal::MotorFactory::Instance().Register(
      "akelc", [](const hal::MotorConfig& c) -> std::unique_ptr<hal::Motor> {
        MotorAkelc::Config ac;
        ac.bus = c.bus;
        ac.device_id = c.id;
        ac.max_speed_rpm = c.max_speed_rpm;
        ac.max_current_a = c.max_current_a;
        return std::make_unique<MotorAkelc>(std::move(ac));
      });
  return true;
}

namespace {
// Self-registration for whole-archive / header-referencing linkage. The
// explicit RegisterAkelcMotor() above is the reliable path for plain static
// linking.
const bool kAkelcMotorRegistered = RegisterAkelcMotor();
}  // namespace

}  // namespace xmotion

/*
 * motor_akelc_modbus.cpp
 *
 * Created on 4/20/24 11:52 PM
 * Description: AKELC holding-register access over Modbus-RTU.
 *
 * Copyright (c) 2024 Ruixiang Du (rdu)
 */

#include "motor_akelc/motor_akelc_modbus.hpp"

#include <cstring>
#include <limits>

#include "xmbase/logging/xlogger.hpp"

namespace xmotion {
namespace {
// state registers
constexpr uint16_t REGISTER_ADDRESS_DEVICE_ID = 0x0000;
constexpr uint16_t REGISTER_ADDRESS_DEVICE_NAME = 0x0002;
constexpr uint16_t REGISTER_ADDRESS_DRIVER_PWM = 0x0010;
constexpr uint16_t REGISTER_ADDRESS_DRIVER_CURRENT = 0x0011;
constexpr uint16_t REGISTER_ADDRESS_SWITCHING_FREQ = 0x0012;

constexpr uint16_t REGISTER_ADDRESS_BLOCKED_STATE = 0x0013;
constexpr uint16_t REGISTER_ADDRESS_ERROR_STATE = 0x0017;

constexpr uint16_t REGISTER_ADDRESS_DRIVER_TEMPERATURE = 0x001c;
constexpr uint16_t REGISTER_ADDRESS_DRIVER_INPUT_VOLTAGE = 0x001d;
constexpr uint16_t REGISTER_ADDRESS_DRIVER_ACTUAL_RPM_HIGH = 0x001e;

// control registers
constexpr uint16_t REGISTER_ADDRESS_CTRL_TARGET_SPEED = 0x0040;
constexpr uint16_t REGISTER_ADDRESS_CTRL_APPLY_BRAKE = 0x0042;
constexpr uint16_t REGISTER_ADDRESS_CTRL_RELEASE_BRAKE = 0x0044;

// system configuration registers
constexpr uint16_t REGISTER_ADDRESS_CONF_KP_INTERNAL_FEEDBACK_HIGH = 0x0090;
constexpr uint16_t REGISTER_ADDRESS_CONF_KI_INTERNAL_FEEDBACK_HIGH = 0x0092;
constexpr uint16_t REGISTER_ADDRESS_CONF_KD_INTERNAL_FEEDBACK_HIGH = 0x0094;

constexpr uint16_t REGISTER_ADDRESS_CONF_KP_EXTERNAL_FEEDBACK_HIGH = 0x0098;
constexpr uint16_t REGISTER_ADDRESS_CONF_KI_EXTERNAL_FEEDBACK_HIGH = 0x009a;
constexpr uint16_t REGISTER_ADDRESS_CONF_KD_EXTERNAL_FEEDBACK_HIGH = 0x009c;

// Pack a 32-bit gain into two consecutive registers, high word first.
// NOTE: `gain` is cast to a 32-bit integer; if the AKELC register expects a
// fixed-point value the caller must pre-scale it (the device register spec was
// not available to confirm the scale here).
void PackGain(uint16_t* out, double gain) {
  const uint32_t v = static_cast<uint32_t>(gain);
  out[0] = static_cast<uint16_t>((v >> 16) & 0xffff);  // high word
  out[1] = static_cast<uint16_t>(v & 0xffff);          // low word
}
}  // namespace

MotorAkelcModbus::MotorAkelcModbus(std::shared_ptr<ModbusRtuInterface> port,
                                   int device_id,
                                   std::chrono::nanoseconds freshness_timeout)
    : ModbusRtuClient(port, device_id), freshness_(freshness_timeout) {}

bool MotorAkelcModbus::IsReachable() {
  if (!port_) return false;
  int ret = port_->ReadHoldingRegisters(device_id_, REGISTER_ADDRESS_DEVICE_ID,
                                        1, buffer_);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to read register {}, error code: {}",
               REGISTER_ADDRESS_DEVICE_ID, ret);
    return false;
  }
  freshness_.Mark();
  XLOG_DEBUG_STREAM("Queried device ID: " << std::hex << buffer_[0]);
  return true;
}

std::string MotorAkelcModbus::GetDeviceName() {
  if (!port_) return "Unknown Device";
  int ret = port_->ReadHoldingRegisters(
      device_id_, REGISTER_ADDRESS_DEVICE_NAME, 8, buffer_);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to read register {}, error code: {}",
               REGISTER_ADDRESS_DEVICE_NAME, ret);
    return "Unknown Device";
  }
  freshness_.Mark();
  // Bound the string to the 8 registers (16 bytes) actually read — a bare
  // std::string{char*} would scan for a NUL past the read bytes into the rest
  // of buffer_ (uninitialized) and potentially past its end.
  const char* name = reinterpret_cast<const char*>(buffer_);
  return std::string(name, strnlen(name, 8 * sizeof(uint16_t)));
}

hal::Status MotorAkelcModbus::SetTargetSwitchingFreq(int16_t freq) {
  if (!port_) return hal::Status::kNotConnected;
  buffer_[0] = static_cast<uint16_t>(freq);
  int ret = port_->WriteSingleRegister(
      device_id_, REGISTER_ADDRESS_SWITCHING_FREQ, buffer_[0]);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to write register {}, error code: {}",
               REGISTER_ADDRESS_SWITCHING_FREQ, ret);
    return hal::Status::kIoError;
  }
  return hal::Status::kOk;
}

hal::Result<int16_t> MotorAkelcModbus::GetActualSwitchingFreq() {
  if (!port_) return hal::Result<int16_t>::Err(hal::Status::kNotConnected);
  int ret = port_->ReadHoldingRegisters(
      device_id_, REGISTER_ADDRESS_SWITCHING_FREQ, 1, buffer_);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to read register {}, error code: {}",
               REGISTER_ADDRESS_SWITCHING_FREQ, ret);
    return hal::Result<int16_t>::Err(hal::Status::kIoError);
  }
  freshness_.Mark();
  return hal::Result<int16_t>::Ok(static_cast<int16_t>(buffer_[0]));
}

hal::Status MotorAkelcModbus::SetTargetRpm(int32_t rpm) {
  if (!port_) return hal::Status::kNotConnected;
  // The target-speed register is 16-bit signed; saturate so a large value does
  // not wrap to the opposite direction on the wire.
  constexpr int32_t kMax = std::numeric_limits<int16_t>::max();
  if (rpm < -kMax) rpm = -kMax;
  if (rpm > kMax) rpm = kMax;
  buffer_[0] = static_cast<uint16_t>(static_cast<int16_t>(rpm));
  int ret = port_->WriteSingleRegister(
      device_id_, REGISTER_ADDRESS_CTRL_TARGET_SPEED, buffer_[0]);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to write register {}, error code: {}",
               REGISTER_ADDRESS_CTRL_TARGET_SPEED, ret);
    return hal::Status::kIoError;
  }
  return hal::Status::kOk;
}

hal::Result<int32_t> MotorAkelcModbus::GetActualRpm() {
  if (!port_) return hal::Result<int32_t>::Err(hal::Status::kNotConnected);
  int ret = port_->ReadHoldingRegisters(
      device_id_, REGISTER_ADDRESS_DRIVER_ACTUAL_RPM_HIGH, 2, buffer_);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to read register {}, error code: {}",
               REGISTER_ADDRESS_DRIVER_ACTUAL_RPM_HIGH, ret);
    return hal::Result<int32_t>::Err(hal::Status::kIoError);
  }
  freshness_.Mark();
  return hal::Result<int32_t>::Ok(
      static_cast<int32_t>((static_cast<uint32_t>(buffer_[0]) << 16) |
                           static_cast<uint32_t>(buffer_[1])));
}

hal::Status MotorAkelcModbus::ApplyBrake(float percentage) {
  if (!port_) return hal::Status::kNotConnected;
  if (percentage < 0) percentage = 0;
  if (percentage > 100) percentage = 100;
  buffer_[0] = static_cast<uint16_t>(percentage * 10);
  int ret = port_->WriteSingleRegister(
      device_id_, REGISTER_ADDRESS_CTRL_APPLY_BRAKE, buffer_[0]);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to write register {}, error code: {}",
               REGISTER_ADDRESS_CTRL_APPLY_BRAKE, ret);
    return hal::Status::kIoError;
  }
  return hal::Status::kOk;
}

hal::Status MotorAkelcModbus::ReleaseBrake() {
  if (!port_) return hal::Status::kNotConnected;
  int ret = port_->WriteSingleRegister(device_id_,
                                       REGISTER_ADDRESS_CTRL_RELEASE_BRAKE, 1);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to write register {}, error code: {}",
               REGISTER_ADDRESS_CTRL_RELEASE_BRAKE, ret);
    return hal::Status::kIoError;
  }
  return hal::Status::kOk;
}

hal::Result<double> MotorAkelcModbus::GetDriverCurrent() {
  if (!port_) return hal::Result<double>::Err(hal::Status::kNotConnected);
  int ret = port_->ReadHoldingRegisters(
      device_id_, REGISTER_ADDRESS_DRIVER_CURRENT, 1, buffer_);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to read register {}, error code: {}",
               REGISTER_ADDRESS_DRIVER_CURRENT, ret);
    return hal::Result<double>::Err(hal::Status::kIoError);
  }
  freshness_.Mark();
  return hal::Result<double>::Ok(buffer_[0] * 0.0001);
}

hal::Result<double> MotorAkelcModbus::GetDriverPwm() {
  if (!port_) return hal::Result<double>::Err(hal::Status::kNotConnected);
  int ret = port_->ReadHoldingRegisters(device_id_, REGISTER_ADDRESS_DRIVER_PWM,
                                        1, buffer_);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to read register {}, error code: {}",
               REGISTER_ADDRESS_DRIVER_PWM, ret);
    return hal::Result<double>::Err(hal::Status::kIoError);
  }
  freshness_.Mark();
  return hal::Result<double>::Ok(buffer_[0] * 0.001);
}

hal::Result<double> MotorAkelcModbus::GetDriverTemperature() {
  if (!port_) return hal::Result<double>::Err(hal::Status::kNotConnected);
  int ret = port_->ReadHoldingRegisters(
      device_id_, REGISTER_ADDRESS_DRIVER_TEMPERATURE, 1, buffer_);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to read register {}, error code: {}",
               REGISTER_ADDRESS_DRIVER_TEMPERATURE, ret);
    return hal::Result<double>::Err(hal::Status::kIoError);
  }
  freshness_.Mark();
  return hal::Result<double>::Ok(static_cast<int16_t>(buffer_[0]) * 0.1);
}

hal::Result<double> MotorAkelcModbus::GetDriverInputVoltage() {
  if (!port_) return hal::Result<double>::Err(hal::Status::kNotConnected);
  int ret = port_->ReadHoldingRegisters(
      device_id_, REGISTER_ADDRESS_DRIVER_INPUT_VOLTAGE, 1, buffer_);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to read register {}, error code: {}",
               REGISTER_ADDRESS_DRIVER_INPUT_VOLTAGE, ret);
    return hal::Result<double>::Err(hal::Status::kIoError);
  }
  freshness_.Mark();
  return hal::Result<double>::Ok(buffer_[0] * 0.1);
}

hal::Result<bool> MotorAkelcModbus::IsMotorBlocked() {
  if (!port_) return hal::Result<bool>::Err(hal::Status::kNotConnected);
  int ret = port_->ReadHoldingRegisters(
      device_id_, REGISTER_ADDRESS_BLOCKED_STATE, 1, buffer_);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to read register {}, error code: {}",
               REGISTER_ADDRESS_BLOCKED_STATE, ret);
    return hal::Result<bool>::Err(hal::Status::kIoError);
  }
  freshness_.Mark();
  return hal::Result<bool>::Ok(buffer_[0] != 0);
}

hal::Result<MotorAkelcModbus::ErrorCode> MotorAkelcModbus::GetErrorCode() {
  if (!port_) return hal::Result<ErrorCode>::Err(hal::Status::kNotConnected);
  int ret = port_->ReadHoldingRegisters(
      device_id_, REGISTER_ADDRESS_ERROR_STATE, 1, buffer_);
  if (ret < 0) {
    XLOG_ERROR("[Akelc Modbus] Failed to read register {}, error code: {}",
               REGISTER_ADDRESS_ERROR_STATE, ret);
    return hal::Result<ErrorCode>::Err(hal::Status::kIoError);
  }
  freshness_.Mark();
  return hal::Result<ErrorCode>::Ok(static_cast<ErrorCode>(buffer_[0]));
}

hal::Status MotorAkelcModbus::ConfigurePidWithInternalFeedback(double kp,
                                                               double ki,
                                                               double kd) {
  if (!port_) return hal::Status::kNotConnected;
  hal::Status status = hal::Status::kOk;
  const struct {
    uint16_t reg;
    double gain;
  } gains[] = {{REGISTER_ADDRESS_CONF_KP_INTERNAL_FEEDBACK_HIGH, kp},
               {REGISTER_ADDRESS_CONF_KI_INTERNAL_FEEDBACK_HIGH, ki},
               {REGISTER_ADDRESS_CONF_KD_INTERNAL_FEEDBACK_HIGH, kd}};
  for (const auto& g : gains) {
    PackGain(buffer_, g.gain);
    if (port_->WriteMultipleRegisters(device_id_, g.reg, 2, buffer_) < 0) {
      XLOG_ERROR("[Akelc Modbus] Failed to write PID register {}", g.reg);
      status = hal::Status::kIoError;
    }
  }
  return status;
}

hal::Status MotorAkelcModbus::ConfigurePidWithExternalFeedback(double kp,
                                                               double ki,
                                                               double kd) {
  if (!port_) return hal::Status::kNotConnected;
  hal::Status status = hal::Status::kOk;
  const struct {
    uint16_t reg;
    double gain;
  } gains[] = {{REGISTER_ADDRESS_CONF_KP_EXTERNAL_FEEDBACK_HIGH, kp},
               {REGISTER_ADDRESS_CONF_KI_EXTERNAL_FEEDBACK_HIGH, ki},
               {REGISTER_ADDRESS_CONF_KD_EXTERNAL_FEEDBACK_HIGH, kd}};
  for (const auto& g : gains) {
    PackGain(buffer_, g.gain);
    if (port_->WriteMultipleRegisters(device_id_, g.reg, 2, buffer_) < 0) {
      XLOG_ERROR("[Akelc Modbus] Failed to write PID register {}", g.reg);
      status = hal::Status::kIoError;
    }
  }
  return status;
}
}  // namespace xmotion

/*
 * @file motor_akelc_modbus.hpp
 * @date 4/20/24
 * @brief Low-level AKELC register access over Modbus-RTU.
 *
 * This layer speaks the AKELC holding-register map. It knows nothing about the
 * HAL capability contract; it just turns register reads/writes into the uniform
 * hal::Status / hal::Result<T> outcome types so the caller can never confuse a
 * bus fault with a real value (the old getters returned 0 on error). Every
 * successful read Mark()s a FreshnessMonitor so staleness is observable.
 *
 * @copyright Copyright (c) 2024 Ruixiang Du (rdu)
 */

#ifndef XMOTION_MOTOR_AKELC_MODBUS_HPP_
#define XMOTION_MOTOR_AKELC_MODBUS_HPP_

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "xmdriver/hal/freshness.hpp"
#include "xmdriver/hal/result.hpp"
#include "xmdriver/hal/status.hpp"
#include "xmdriver/transport/modbus_rtu_client.hpp"

namespace xmotion {
class MotorAkelcModbus final : public ModbusRtuClient {
 public:
  enum class ErrorCode : int {
    kNoError = 0,
    kBlocked = 2,
    kUnableToReachTargetRpm = 4,
    kOverCurrentStop = 6,
    kOverHeatStop = 7,
    kOverVoltageStop = 8,
    kUnderVoltageStop = 9,
    kCircuitShortStop = 10,
    kBrakingCurrentException = 11,
    kUnknown = 20
  };

  // `freshness_timeout` bounds how long a successful read stays "fresh" before
  // Health()/reads treat the link as stale (default 200 ms — a few missed 20 Hz
  // polls, matching the SBUS watchdog reference).
  MotorAkelcModbus(std::shared_ptr<ModbusRtuInterface> port, int device_id,
                   std::chrono::nanoseconds freshness_timeout =
                       std::chrono::milliseconds(200));
  ~MotorAkelcModbus() override = default;

  // --- ModbusRtuClient ---
  bool IsReachable() override;

  std::string GetDeviceName();

  // Commands: a failed transport write maps to kIoError.
  hal::Status SetTargetSwitchingFreq(int16_t freq);
  hal::Status SetTargetRpm(int32_t rpm);
  hal::Status ApplyBrake(float percentage);
  hal::Status ReleaseBrake();
  hal::Status ConfigurePidWithInternalFeedback(double kp, double ki, double kd);
  hal::Status ConfigurePidWithExternalFeedback(double kp, double ki, double kd);

  // Reads: never a fake 0 — a failed read returns kIoError, a real value only
  // when the transport actually answered (which also Mark()s freshness).
  hal::Result<int16_t> GetActualSwitchingFreq();
  hal::Result<int32_t> GetActualRpm();
  hal::Result<double> GetDriverCurrent();
  hal::Result<double> GetDriverPwm();
  hal::Result<double> GetDriverTemperature();
  hal::Result<double> GetDriverInputVoltage();
  hal::Result<bool> IsMotorBlocked();
  hal::Result<ErrorCode> GetErrorCode();

  const hal::FreshnessMonitor& freshness() const { return freshness_; }

 private:
  hal::FreshnessMonitor freshness_;
  uint16_t buffer_[16];
};
}  // namespace xmotion

#endif  // XMOTION_MOTOR_AKELC_MODBUS_HPP_

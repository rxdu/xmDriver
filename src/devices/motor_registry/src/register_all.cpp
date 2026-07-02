/*
 * register_all.cpp
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "motor_registry/register_all.hpp"

#include "motor_akelc/motor_akelc.hpp"
#include "motor_vesc/vesc_motor.hpp"
#include "motor_waveshare/ddsm_210.hpp"
#include "motor_waveshare/sms_sts_servo.hpp"

namespace xmotion {

bool RegisterAllMotors() {
  RegisterVescMotor();
  RegisterAkelcMotor();
  RegisterDdsm210Motor();
  RegisterSmsStsServo();
  return true;
}

}  // namespace xmotion

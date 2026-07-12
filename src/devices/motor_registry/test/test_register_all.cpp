/*
 * test_register_all.cpp — RegisterAllMotors() registers every bundled driver
 * with the factory, robust against static-lib initializer GC (ADR 0003).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "motor_registry/register_all.hpp"

#include "gtest/gtest.h"

#include "xmdriver/hal/motor_factory.hpp"

using namespace xmotion;

TEST(RegisterAllMotors, RegistersEveryBundledType) {
  RegisterAllMotors();
  const auto& factory = hal::MotorFactory::Instance();
  EXPECT_TRUE(factory.IsRegistered("vesc"));
  EXPECT_TRUE(factory.IsRegistered("akelc"));
  EXPECT_TRUE(factory.IsRegistered("ddsm210"));
  EXPECT_TRUE(factory.IsRegistered("ddsm210_array"));
  EXPECT_TRUE(factory.IsRegistered("sms_sts"));
  EXPECT_TRUE(factory.IsRegistered("sms_sts_array"));
  EXPECT_TRUE(factory.IsRegistered("sim"));
}

TEST(RegisterAllMotors, SimMotorIsConstructibleFromConfig) {
  RegisterAllMotors();
  hal::MotorConfig cfg;
  cfg.type = "sim";
  cfg.max_speed_rpm = 1000.0;
  auto motor = hal::MotorFactory::Instance().Create(cfg);
  ASSERT_NE(motor, nullptr);
  EXPECT_EQ(motor->Connect(), hal::Status::kOk);
}

TEST(RegisterAllMotors, UnknownTypeCreatesNull) {
  RegisterAllMotors();
  hal::MotorConfig cfg;
  cfg.type = "no_such_driver";
  EXPECT_EQ(hal::MotorFactory::Instance().Create(cfg), nullptr);
}

/*
 * test_register_all.cpp — RegisterAllMotors() registers every bundled driver
 * with the factory, robust against static-lib initializer GC (ADR 0003).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "motor_registry/register_all.hpp"

#include "gtest/gtest.h"

#include "xmmu/hal/motor_factory.hpp"

using namespace xmotion;

TEST(RegisterAllMotors, RegistersEveryBundledType) {
  RegisterAllMotors();
  const auto& factory = hal::MotorFactory::Instance();
  EXPECT_TRUE(factory.IsRegistered("vesc"));
  EXPECT_TRUE(factory.IsRegistered("akelc"));
  EXPECT_TRUE(factory.IsRegistered("ddsm210"));
  EXPECT_TRUE(factory.IsRegistered("sms_sts"));
}

TEST(RegisterAllMotors, UnknownTypeCreatesNull) {
  RegisterAllMotors();
  hal::MotorConfig cfg;
  cfg.type = "no_such_driver";
  EXPECT_EQ(hal::MotorFactory::Instance().Create(cfg), nullptr);
}

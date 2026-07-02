/*
 * test_vesc_motor.cpp — VescMotor on the new HAL, hardware-free properties.
 *
 * Verifies the contract a real CAN bus is not needed for: factory registration,
 * capability set, and that a disconnected motor reports kNotConnected rather
 * than acting on commands or returning a fake 0. (Behavior that needs a live
 * VESC — clamping a sent command, feedback values — is covered on hardware.)
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <memory>

#include "gtest/gtest.h"

#include "xmmu/hal/motor_factory.hpp"
#include "motor_vesc/vesc_motor.hpp"

using namespace xmotion;

TEST(VescMotorTest, RegistersWithFactory) {
  ASSERT_TRUE(RegisterVescMotor());
  EXPECT_TRUE(hal::MotorFactory::Instance().IsRegistered("vesc"));

  hal::MotorConfig cfg;
  cfg.type = "vesc";
  cfg.bus = "can0";
  cfg.id = 3;
  std::unique_ptr<hal::Motor> m = hal::MotorFactory::Instance().Create(cfg);
  ASSERT_NE(m, nullptr);
  EXPECT_FALSE(m->IsConnected());
  EXPECT_EQ(m->Health().state, hal::DeviceHealth::State::kDisconnected);
}

TEST(VescMotorTest, ExposesSpeedCurrentBrakeButNotPosition) {
  VescMotor m({"can0", 1, 0.0, 0.0});
  hal::Motor* base = &m;
  EXPECT_NE(dynamic_cast<hal::SpeedControllable*>(base), nullptr);
  EXPECT_NE(dynamic_cast<hal::CurrentControllable*>(base), nullptr);
  EXPECT_NE(dynamic_cast<hal::Brakable*>(base), nullptr);
  EXPECT_EQ(dynamic_cast<hal::PositionControllable*>(base), nullptr);
}

TEST(VescMotorTest, CommandsAndReadsRejectedWhenDisconnected) {
  VescMotor m({"can0", 1, 6000.0, 20.0});
  EXPECT_FALSE(m.IsConnected());

  // Commands do not reach hardware and report kNotConnected (no silent action).
  EXPECT_EQ(m.SetSpeed(hal::Rpm{1000.0}), hal::Status::kNotConnected);
  EXPECT_EQ(m.SetCurrent(hal::Ampere{5.0}), hal::Status::kNotConnected);
  EXPECT_EQ(m.Stop(), hal::Status::kNotConnected);

  // Reads return a status, never a fake 0.
  hal::Result<hal::Rpm> s = m.GetSpeed();
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.status, hal::Status::kNotConnected);
}

TEST(VescMotorTest, ConnectToNonexistentBusFailsCleanly) {
  // No such CAN interface in CI -> Connect must fail with a status, not crash,
  // and leave the motor disconnected.
  VescMotor m({"xmnotacan0", 1, 6000.0, 20.0});
  EXPECT_NE(m.Connect(), hal::Status::kOk);
  EXPECT_FALSE(m.IsConnected());
}

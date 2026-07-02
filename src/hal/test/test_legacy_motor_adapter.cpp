/*
 * test_legacy_motor_adapter.cpp
 *
 * Verifies LegacyMotorAdapter forwards the legacy MotorControllerInterface calls
 * to a new-HAL motor (the reference SimMotorController), and throws for
 * capabilities the motor does not implement.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <memory>

#include "gtest/gtest.h"

#include "xmmu/hal/compat/legacy_motor_adapter.hpp"
#include "xmmu/hal/reference/sim_motor_controller.hpp"

using namespace xmotion;

namespace {
// SimMotorController implements Speed + Current + Brake (+ SafetyStoppable), but
// NOT Position or Torque — exactly what we want to exercise both branches.
std::shared_ptr<hal::Motor> MakeConnectedSim() {
  auto sim = std::make_shared<hal::SimMotorController>(1000.0, 20.0);
  sim->Connect();
  return sim;
}
}  // namespace

TEST(LegacyMotorAdapterTest, ForwardsSpeedThroughLegacyInterface) {
  LegacyMotorAdapter adapter(MakeConnectedSim());
  // Drive it through the legacy interface, as xmNabla's actuator groups do.
  MotorControllerInterface& m = adapter;
  m.SetSpeed(250.0f);
  EXPECT_FLOAT_EQ(m.GetSpeed(), 250.0f);
}

TEST(LegacyMotorAdapterTest, ForwardsBrakeAndHealth) {
  LegacyMotorAdapter m(MakeConnectedSim());
  m.SetSpeed(500.0f);
  m.ApplyBrake(1.0f);  // full brake -> sim bleeds speed to 0
  EXPECT_FLOAT_EQ(m.GetSpeed(), 0.0f);
  EXPECT_TRUE(m.IsNormal());  // connected sim reports healthy
}

TEST(LegacyMotorAdapterTest, UnsupportedCapabilityThrows) {
  LegacyMotorAdapter m(MakeConnectedSim());
  // Sim has no position/torque control -> legacy "not implemented" contract.
  EXPECT_THROW(m.SetPosition(1.0f), std::runtime_error);
  EXPECT_THROW(m.SetTorque(1.0f), std::runtime_error);
  // Speed/current/brake are supported -> no throw.
  EXPECT_NO_THROW(m.SetSpeed(100.0f));
  EXPECT_NO_THROW(m.SetCurrent(5.0f));
}

TEST(LegacyMotorAdapterTest, HealthFalseWhenDisconnected) {
  auto sim = std::make_shared<hal::SimMotorController>();  // not connected
  LegacyMotorAdapter m(sim);
  EXPECT_FALSE(m.IsNormal());
}

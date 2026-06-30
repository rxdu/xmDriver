/*
 * test_hal_interfaces.cpp
 *
 * Properties of the redesigned HAL interface layer, exercised through the
 * reference SimMotorController:
 *   - config-driven construction (the app names no concrete driver),
 *   - capability discovery via dynamic_cast (no throwing for unsupported ops),
 *   - strong unit types, clamp-AND-report (no silent saturation), no exceptions,
 *   - mandatory failsafe Stop() on every actuator,
 *   - reads return Result (a failed read is never a fake 0).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <cmath>
#include <memory>
#include <type_traits>

#include "gtest/gtest.h"

#include "xmmu/hal/motor_factory.hpp"
#include "xmmu/hal/reference/sim_motor_controller.hpp"

using namespace xmotion::hal;

TEST(HalInterfaceTest, FactoryBuildsFromConfigWithoutNamingDriver) {
  MotorConfig cfg;
  cfg.type = "sim";
  cfg.max_speed_rpm = 1000.0;
  cfg.max_current_a = 15.0;

  std::unique_ptr<Motor> motor = MotorFactory::Instance().Create(cfg);
  ASSERT_NE(motor, nullptr) << "sim driver must self-register";
  EXPECT_EQ(motor->Connect(), Status::kOk);
  EXPECT_TRUE(motor->IsConnected());
  EXPECT_EQ(motor->Health().state, DeviceHealth::State::kOk);
}

TEST(HalInterfaceTest, UnknownTypeReturnsNullNotThrow) {
  MotorConfig cfg;
  cfg.type = "no_such_driver";
  EXPECT_EQ(MotorFactory::Instance().Create(cfg), nullptr);
}

TEST(HalInterfaceTest, CapabilityDiscoveryIsTypeBased) {
  // Hold the device through the base handle, as a real caller would — capability
  // discovery is then a genuine runtime query, not a statically-known cast.
  std::unique_ptr<Motor> m =
      MotorFactory::Instance().Create(MotorConfig{"sim", "", 0, 0.0, 0.0, {}});
  ASSERT_NE(m, nullptr);
  // The sim supports speed/current/brake but NOT position — and that absence is
  // expressed in the type system, not via a thrown exception.
  EXPECT_NE(dynamic_cast<SpeedControllable*>(m.get()), nullptr);
  EXPECT_NE(dynamic_cast<CurrentControllable*>(m.get()), nullptr);
  EXPECT_EQ(dynamic_cast<PositionControllable*>(m.get()), nullptr);
}

TEST(HalInterfaceTest, CommandsClampAndReportNeverThrow) {
  SimMotorController m(1000.0, 15.0);
  m.Connect();
  auto& s = static_cast<SpeedControllable&>(m);

  EXPECT_EQ(s.SetSpeed(Rpm{120.0}), Status::kOk);
  EXPECT_DOUBLE_EQ(s.GetSpeed().value_or(Rpm{}).value(), 120.0);

  // Out of range: clamped to the configured envelope AND reported.
  EXPECT_EQ(s.SetSpeed(Rpm{99999.0}), Status::kOutOfRange);
  EXPECT_DOUBLE_EQ(s.GetSpeed().value_or(Rpm{}).value(), 1000.0);

  // Non-finite input rejected, not propagated into a command.
  EXPECT_EQ(s.SetSpeed(Rpm{std::nan("")}), Status::kInvalidArgument);
}

TEST(HalInterfaceTest, EveryActuatorHasFailsafeStop) {
  SimMotorController m;
  m.Connect();
  auto& s = static_cast<SpeedControllable&>(m);
  s.SetSpeed(Rpm{500.0});
  EXPECT_EQ(static_cast<SafetyStoppable&>(m).Stop(), Status::kOk);
  EXPECT_DOUBLE_EQ(s.GetSpeed().value_or(Rpm{}).value(), 0.0);
}

TEST(HalInterfaceTest, ReadOnDisconnectedReportsStatusNotFakeZero) {
  SimMotorController m;
  // never Connect()
  Result<Rpm> r = m.GetSpeed();
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.status, Status::kNotConnected);
}

TEST(HalInterfaceTest, UnitTypesAreDistinct) {
  static_assert(!std::is_same<Rpm, Ampere>::value,
                "unit types must be distinct");
  // Rpm{} is not convertible to Ampere — a quantity mix-up won't compile.
  static_assert(!std::is_convertible<Rpm, Ampere>::value,
                "unit types must not implicitly convert");
}

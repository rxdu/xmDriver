/*
 * test_motor_akelc.cpp — MotorAkelc on the new HAL, driven by a fake Modbus
 * transport (no hardware). Covers factory registration, capability set,
 * disconnected behavior, NaN rejection, clamp-and-report, Stop() safe state,
 * and reads that surface an I/O error instead of a fake 0.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>

#include "gtest/gtest.h"

#include "xmmu/hal/motor_factory.hpp"
#include "motor_akelc/motor_akelc.hpp"
#include "motor_akelc/motor_akelc_modbus.hpp"

using namespace xmotion;

namespace {

// AKELC register addresses (mirrors the private map in motor_akelc_modbus.cpp).
constexpr uint16_t kRegDeviceId = 0x0000;
constexpr uint16_t kRegBlocked = 0x0013;
constexpr uint16_t kRegError = 0x0017;
constexpr uint16_t kRegRpmHigh = 0x001e;
constexpr uint16_t kRegRpmLow = 0x001f;
constexpr uint16_t kRegTargetSpeed = 0x0040;
constexpr uint16_t kRegApplyBrake = 0x0042;
constexpr uint16_t kRegReleaseBrake = 0x0044;

// A minimal in-memory ModbusRtuInterface: reads pull from `regs`, writes land
// in `writes`. `read_ok`/`write_ok` flip to simulate a bus fault.
class FakeModbus : public ModbusRtuInterface {
 public:
  std::map<uint16_t, uint16_t> regs;
  std::map<uint16_t, uint16_t> writes;
  bool open_ok = true;
  bool read_ok = true;
  bool write_ok = true;

  bool Open(const std::string&, int, Parity, DataBit, StopBit) override {
    opened_ = open_ok;
    return open_ok;
  }
  void Close() override { opened_ = false; }
  bool IsOpened() const override { return opened_; }
  bool SetResponseTimeout(int, int) override { return true; }

  int ReadHoldingRegisters(uint8_t, uint16_t addr, uint16_t quantity,
                           uint16_t* data) override {
    if (!read_ok) return -1;
    for (uint16_t i = 0; i < quantity; ++i) {
      auto it = regs.find(static_cast<uint16_t>(addr + i));
      data[i] = it == regs.end() ? 0 : it->second;
    }
    return quantity;
  }
  int WriteSingleRegister(uint8_t, uint16_t addr, uint16_t value) override {
    if (!write_ok) return -1;
    writes[addr] = value;
    return 1;
  }
  int WriteMultipleRegisters(uint8_t, uint16_t addr, uint16_t quantity,
                             const uint16_t* data) override {
    if (!write_ok) return -1;
    for (uint16_t i = 0; i < quantity; ++i)
      writes[static_cast<uint16_t>(addr + i)] = data[i];
    return quantity;
  }

  // Unused read/write paths.
  int ReadCoils(uint8_t, uint16_t, uint16_t, uint8_t*) override { return -1; }
  int WriteSingleCoil(uint8_t, uint16_t, int) override { return -1; }
  int WriteMultipleCoils(uint8_t, uint16_t, uint16_t,
                         const uint8_t*) override {
    return -1;
  }
  int ReadDiscreteInputs(uint8_t, uint16_t, uint16_t, uint8_t*) override {
    return -1;
  }
  int ReadInputRegisters(uint8_t, uint16_t, uint16_t, uint16_t*) override {
    return -1;
  }

 protected:
  bool SelectDevice(uint8_t) override { return true; }

 private:
  bool opened_ = false;
};

// Connect a MotorAkelc backed by the given fake transport.
std::unique_ptr<MotorAkelc> MakeConnected(std::shared_ptr<FakeModbus> fake,
                                          double max_speed_rpm = 1000.0) {
  MotorAkelc::Config cfg;
  cfg.bus = "/dev/fake";
  cfg.device_id = 5;
  cfg.max_speed_rpm = max_speed_rpm;
  auto m = std::make_unique<MotorAkelc>(cfg, fake);
  EXPECT_EQ(m->Connect(), hal::Status::kOk);
  EXPECT_TRUE(m->IsConnected());
  return m;
}

}  // namespace

TEST(MotorAkelcTest, RegistersWithFactory) {
  ASSERT_TRUE(RegisterAkelcMotor());
  EXPECT_TRUE(hal::MotorFactory::Instance().IsRegistered("akelc"));

  hal::MotorConfig cfg;
  cfg.type = "akelc";
  cfg.bus = "/dev/ttyUSB0";
  cfg.id = 5;
  std::unique_ptr<hal::Motor> m = hal::MotorFactory::Instance().Create(cfg);
  ASSERT_NE(m, nullptr);
  EXPECT_FALSE(m->IsConnected());
  EXPECT_EQ(m->Health().state, hal::DeviceHealth::State::kDisconnected);
}

TEST(MotorAkelcTest, ExposesSpeedAndBrakeButNotCurrentOrPosition) {
  MotorAkelc m(MotorAkelc::Config{});
  hal::Motor* base = &m;
  EXPECT_NE(dynamic_cast<hal::SpeedControllable*>(base), nullptr);
  EXPECT_NE(dynamic_cast<hal::Brakable*>(base), nullptr);
  EXPECT_EQ(dynamic_cast<hal::CurrentControllable*>(base), nullptr);
  EXPECT_EQ(dynamic_cast<hal::PositionControllable*>(base), nullptr);
}

TEST(MotorAkelcTest, CommandsAndReadsRejectedWhenDisconnected) {
  MotorAkelc m(MotorAkelc::Config{});
  EXPECT_FALSE(m.IsConnected());
  EXPECT_EQ(m.SetSpeed(hal::Rpm{100.0}), hal::Status::kNotConnected);
  EXPECT_EQ(m.Stop(), hal::Status::kNotConnected);
  EXPECT_EQ(m.ApplyBrake(hal::Ratio{0.5}), hal::Status::kNotConnected);

  auto s = m.GetSpeed();
  EXPECT_FALSE(s.ok());
  EXPECT_EQ(s.status, hal::Status::kNotConnected);
}

TEST(MotorAkelcTest, ConnectFailsWhenDeviceUnreachable) {
  auto fake = std::make_shared<FakeModbus>();
  fake->read_ok = false;  // IsReachable() read fails
  MotorAkelc::Config cfg;
  cfg.bus = "/dev/fake";
  cfg.device_id = 5;
  MotorAkelc m(cfg, fake);
  EXPECT_EQ(m.Connect(), hal::Status::kTimeout);
  EXPECT_FALSE(m.IsConnected());
}

TEST(MotorAkelcTest, SetSpeedWithinRangeWritesTargetRegister) {
  auto fake = std::make_shared<FakeModbus>();
  fake->regs[kRegDeviceId] = 0x0101;
  auto m = MakeConnected(fake);

  EXPECT_EQ(m->SetSpeed(hal::Rpm{500.0}), hal::Status::kOk);
  ASSERT_TRUE(fake->writes.count(kRegTargetSpeed));
  EXPECT_EQ(static_cast<int16_t>(fake->writes[kRegTargetSpeed]), 500);
}

TEST(MotorAkelcTest, SetSpeedRejectsNonFinite) {
  auto fake = std::make_shared<FakeModbus>();
  auto m = MakeConnected(fake);
  EXPECT_EQ(m->SetSpeed(hal::Rpm{std::nan("")}), hal::Status::kInvalidArgument);
  EXPECT_EQ(m->SetSpeed(hal::Rpm{INFINITY}), hal::Status::kInvalidArgument);
  // Rejected commands must not touch the bus.
  EXPECT_FALSE(fake->writes.count(kRegTargetSpeed));
}

TEST(MotorAkelcTest, SetSpeedClampsAndReportsOutOfRange) {
  auto fake = std::make_shared<FakeModbus>();
  auto m = MakeConnected(fake, /*max_speed_rpm=*/1000.0);

  EXPECT_EQ(m->SetSpeed(hal::Rpm{5000.0}), hal::Status::kOutOfRange);
  EXPECT_EQ(static_cast<int16_t>(fake->writes[kRegTargetSpeed]), 1000);

  EXPECT_EQ(m->SetSpeed(hal::Rpm{-5000.0}), hal::Status::kOutOfRange);
  EXPECT_EQ(static_cast<int16_t>(fake->writes[kRegTargetSpeed]), -1000);
}

TEST(MotorAkelcTest, StopZerosTargetRpm) {
  auto fake = std::make_shared<FakeModbus>();
  auto m = MakeConnected(fake);
  fake->writes.clear();
  EXPECT_EQ(m->Stop(), hal::Status::kOk);
  ASSERT_TRUE(fake->writes.count(kRegTargetSpeed));
  EXPECT_EQ(fake->writes[kRegTargetSpeed], 0);
}

TEST(MotorAkelcTest, DisconnectCommandsStopBeforeClosing) {
  auto fake = std::make_shared<FakeModbus>();
  auto m = MakeConnected(fake);
  fake->writes.clear();
  m->Disconnect();
  EXPECT_FALSE(m->IsConnected());
  ASSERT_TRUE(fake->writes.count(kRegTargetSpeed));  // Stop() ran on teardown
  EXPECT_EQ(fake->writes[kRegTargetSpeed], 0);
  EXPECT_FALSE(fake->IsOpened());
}

TEST(MotorAkelcTest, GetSpeedReturnsValueThenIoErrorOnBusFault) {
  auto fake = std::make_shared<FakeModbus>();
  fake->regs[kRegRpmHigh] = 0;
  fake->regs[kRegRpmLow] = 250;
  auto m = MakeConnected(fake);

  auto ok = m->GetSpeed();
  ASSERT_TRUE(ok.ok());
  EXPECT_DOUBLE_EQ(ok.value.value(), 250.0);

  fake->read_ok = false;  // bus goes dark
  auto err = m->GetSpeed();
  EXPECT_FALSE(err.ok());
  EXPECT_EQ(err.status, hal::Status::kIoError);  // never a fake 0
}

TEST(MotorAkelcTest, ApplyBrakeValidatesRatio) {
  auto fake = std::make_shared<FakeModbus>();
  auto m = MakeConnected(fake);

  EXPECT_EQ(m->ApplyBrake(hal::Ratio{std::nan("")}),
            hal::Status::kInvalidArgument);
  EXPECT_EQ(m->ApplyBrake(hal::Ratio{1.5}), hal::Status::kOutOfRange);
  EXPECT_EQ(m->ApplyBrake(hal::Ratio{-0.1}), hal::Status::kOutOfRange);

  EXPECT_EQ(m->ApplyBrake(hal::Ratio{0.5}), hal::Status::kOk);
  ASSERT_TRUE(fake->writes.count(kRegApplyBrake));
  EXPECT_EQ(fake->writes[kRegApplyBrake], 500);  // 0.5 -> 50% -> *10

  EXPECT_EQ(m->ReleaseBrake(), hal::Status::kOk);
  EXPECT_TRUE(fake->writes.count(kRegReleaseBrake));
}

TEST(MotorAkelcTest, HealthFaultsOnDeviceErrorRegister) {
  auto fake = std::make_shared<FakeModbus>();
  auto m = MakeConnected(fake);
  EXPECT_EQ(m->Health().state, hal::DeviceHealth::State::kOk);

  fake->regs[kRegError] =
      static_cast<uint16_t>(MotorAkelcModbus::ErrorCode::kOverHeatStop);
  EXPECT_EQ(m->Health().state, hal::DeviceHealth::State::kFault);

  fake->regs[kRegError] = 0;
  fake->regs[kRegBlocked] = 1;
  EXPECT_EQ(m->Health().state, hal::DeviceHealth::State::kFault);
}

TEST(MotorAkelcModbusTest, FreshnessGoesStaleAfterTimeout) {
  auto fake = std::make_shared<FakeModbus>();
  fake->regs[kRegDeviceId] = 0x0101;
  // Zero-length freshness window: fresh only at the instant of the read.
  MotorAkelcModbus dev(fake, 5, std::chrono::nanoseconds(0));
  EXPECT_TRUE(dev.IsReachable());   // Mark()s freshness
  EXPECT_FALSE(dev.freshness().IsFresh());  // already stale
  EXPECT_TRUE(dev.freshness().EverUpdated());
}

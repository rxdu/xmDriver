/*
 * test_motor_waveshare.cpp — Waveshare DDSM-210 & SMS/STS on the new HAL.
 *
 * DDSM-210 is exercised against a fake SerialInterface (no hardware): factory
 * registration, capability set, NaN rejection, clamp-and-report, stale-read
 * (kTimeout until a valid frame arrives), Stop, and Disconnect commanding the
 * safe state before closing. SMS/STS uses a vendored termios serial internally,
 * so only the hardware-free contract (registration, capabilities, disconnected
 * behavior) is checked here.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "gtest/gtest.h"

#include "xmmu/hal/motor_factory.hpp"
#include "xmmu/transport/serial_interface.hpp"
#include "xmsigma/serialization/checksum.hpp"

#include "motor_waveshare/ddsm_210.hpp"
#include "motor_waveshare/sms_sts_servo.hpp"

using namespace xmotion;

namespace {

// Minimal in-memory SerialInterface: records what the driver sends and lets a
// test push RX bytes into the driver's receive callback.
class FakeSerial : public SerialInterface {
 public:
  void SetBaudRate(unsigned) override {}
  void SetHardwareFlowControl(bool) override {}
  bool Open() override {
    opened_ = true;
    return true;
  }
  void Close() override { opened_ = false; }
  bool IsOpened() const override { return opened_; }
  void SetReceiveCallback(ReceiveCallback cb) override { rcv_ = std::move(cb); }
  void SetErrorCallback(ErrorCallback cb) override { err_ = std::move(cb); }
  TransportStatus SendBytes(const uint8_t* b, size_t n) override {
    ++sent_count;
    last_sent.assign(b, b + n);
    return send_status;
  }

  void Deliver(std::vector<uint8_t> bytes) {
    if (rcv_) rcv_(bytes.data(), bytes.size(), bytes.size());
  }

  bool opened_ = false;
  std::size_t sent_count = 0;
  std::vector<uint8_t> last_sent;
  TransportStatus send_status = TransportStatus::kOk;
  ReceiveCallback rcv_;
  ErrorCallback err_;
};

// Build a valid DDSM-210 speed-feedback (0x64) frame for the given id/rpm.
std::vector<uint8_t> MakeSpeedFrame(uint8_t id, int16_t rpm) {
  std::vector<uint8_t> f(10, 0);
  f[0] = id;
  f[1] = 0x64;
  f[2] = static_cast<uint8_t>((rpm & 0xff00) >> 8);
  f[3] = static_cast<uint8_t>(rpm & 0x00ff);
  f[9] = serialization::crc8_maxim(f.data(), 9);
  return f;
}

}  // namespace

TEST(Ddsm210Test, RegistersWithFactory) {
  ASSERT_TRUE(RegisterDdsm210Motor());
  EXPECT_TRUE(hal::MotorFactory::Instance().IsRegistered("ddsm210"));

  hal::MotorConfig cfg;
  cfg.type = "ddsm210";
  cfg.bus = "/dev/ttyUSB0";
  cfg.id = 1;
  auto m = hal::MotorFactory::Instance().Create(cfg);
  ASSERT_NE(m, nullptr);
  EXPECT_FALSE(m->IsConnected());
  EXPECT_EQ(m->Health().state, hal::DeviceHealth::State::kDisconnected);
}

TEST(Ddsm210Test, ExposesSpeedAndPositionOnly) {
  Ddsm210 m({"/dev/ttyUSB0", 1, 0.0});
  hal::Motor* base = &m;
  EXPECT_NE(dynamic_cast<hal::SpeedControllable*>(base), nullptr);
  EXPECT_NE(dynamic_cast<hal::PositionControllable*>(base), nullptr);
  EXPECT_EQ(dynamic_cast<hal::CurrentControllable*>(base), nullptr);
  EXPECT_EQ(dynamic_cast<hal::Brakable*>(base), nullptr);
}

TEST(Ddsm210Test, DisconnectedCommandsAndReadsRejected) {
  Ddsm210 m({"/dev/ttyUSB0", 1, 100.0});
  EXPECT_EQ(m.SetSpeed(hal::Rpm{50.0}), hal::Status::kNotConnected);
  EXPECT_EQ(m.SetPosition(hal::Radian{1.0}), hal::Status::kNotConnected);
  EXPECT_EQ(m.Stop(), hal::Status::kNotConnected);
  EXPECT_EQ(m.GetSpeed().status, hal::Status::kNotConnected);
  EXPECT_EQ(m.GetPosition().status, hal::Status::kNotConnected);
}

TEST(Ddsm210Test, RejectsNonFiniteAndClampsToEnvelope) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210 m({"/dev/ttyUSB0", 1, 100.0}, fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  EXPECT_TRUE(m.IsConnected());

  EXPECT_EQ(m.SetSpeed(hal::Rpm{std::nan("")}), hal::Status::kInvalidArgument);
  EXPECT_EQ(m.SetSpeed(hal::Rpm{INFINITY}), hal::Status::kInvalidArgument);
  EXPECT_EQ(m.SetSpeed(hal::Rpm{50.0}), hal::Status::kOk);
  EXPECT_EQ(m.SetSpeed(hal::Rpm{500.0}), hal::Status::kOutOfRange);  // clamped

  EXPECT_EQ(m.SetPosition(hal::Radian{std::nan("")}),
            hal::Status::kInvalidArgument);
  EXPECT_EQ(m.SetPosition(hal::Radian{0.5}), hal::Status::kOk);
  EXPECT_EQ(m.SetPosition(hal::Radian{100.0}),
            hal::Status::kOutOfRange);  // >360 deg
}

TEST(Ddsm210Test, ReportsTransportFailureOnSend) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210 m({"/dev/ttyUSB0", 1, 100.0}, fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  fake->send_status = TransportStatus::kIoError;
  EXPECT_EQ(m.SetSpeed(hal::Rpm{50.0}), hal::Status::kIoError);
}

TEST(Ddsm210Test, StaleReadUntilFeedbackArrives) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210 m({"/dev/ttyUSB0", 1, 100.0}, fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  // No feedback yet -> not fresh.
  EXPECT_EQ(m.GetSpeed().status, hal::Status::kTimeout);

  // A valid frame makes the read fresh with the decoded value.
  fake->Deliver(MakeSpeedFrame(1, 100));  // rpm field 100 -> 10.0 RPM
  auto s = m.GetSpeed();
  ASSERT_EQ(s.status, hal::Status::kOk);
  EXPECT_DOUBLE_EQ(s.value.value(), 10.0);
}

TEST(Ddsm210Test, StopCommandsZeroSpeed) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210 m({"/dev/ttyUSB0", 1, 100.0}, fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  const std::size_t before = fake->sent_count;
  EXPECT_EQ(m.Stop(), hal::Status::kOk);
  EXPECT_GT(fake->sent_count, before);
}

TEST(Ddsm210Test, DisconnectCommandsSafeStateThenCloses) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210 m({"/dev/ttyUSB0", 1, 100.0}, fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  const std::size_t before = fake->sent_count;
  m.Disconnect();
  EXPECT_GT(fake->sent_count, before);  // Stop() was commanded
  EXPECT_FALSE(fake->opened_);          // port closed after
  EXPECT_FALSE(m.IsConnected());
}

TEST(SmsStsServoTest, RegistersWithFactory) {
  ASSERT_TRUE(RegisterSmsStsServo());
  EXPECT_TRUE(hal::MotorFactory::Instance().IsRegistered("sms_sts"));

  hal::MotorConfig cfg;
  cfg.type = "sms_sts";
  cfg.bus = "/dev/ttyUSB0";
  cfg.id = 1;
  auto m = hal::MotorFactory::Instance().Create(cfg);
  ASSERT_NE(m, nullptr);
  EXPECT_FALSE(m->IsConnected());
  EXPECT_EQ(m->Health().state, hal::DeviceHealth::State::kDisconnected);
}

TEST(SmsStsServoTest, ExposesPositionAndSpeed) {
  SmsStsServo m({"/dev/ttyUSB0", 1, 1000000, 0.0, 0.0});
  hal::Motor* base = &m;
  EXPECT_NE(dynamic_cast<hal::PositionControllable*>(base), nullptr);
  EXPECT_NE(dynamic_cast<hal::SpeedControllable*>(base), nullptr);
  EXPECT_EQ(dynamic_cast<hal::CurrentControllable*>(base), nullptr);
}

TEST(SmsStsServoTest, DisconnectedCommandsAndReadsRejected) {
  SmsStsServo m({"/dev/ttyUSB0", 1, 1000000, 0.0, 0.0});
  EXPECT_EQ(m.SetPosition(hal::Radian{1.0}), hal::Status::kNotConnected);
  EXPECT_EQ(m.SetSpeed(hal::Rpm{100.0}), hal::Status::kNotConnected);
  EXPECT_EQ(m.Stop(), hal::Status::kNotConnected);
  EXPECT_EQ(m.GetPosition().status, hal::Status::kNotConnected);
  EXPECT_EQ(m.GetState().status, hal::Status::kNotConnected);
}

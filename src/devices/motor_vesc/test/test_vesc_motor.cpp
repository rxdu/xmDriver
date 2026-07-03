/*
 * test_vesc_motor.cpp — VescMotor on the new HAL.
 *
 * Two layers of coverage:
 *   1. Hardware-free properties needing no transport: factory registration,
 *      capability set, and that a disconnected motor reports kNotConnected
 *      rather than acting on commands or returning a fake 0.
 *   2. Behavioral coverage against a FakeCan transport injected via the
 *      test ctor — NaN/inf rejection, clamp-and-report (asserting the clamped
 *      command reached the bus), stale reads (kTimeout / kDegraded until a VESC
 *      status frame arrives), Stop() as the safe state, Disconnect() commanding
 *      the safe state before closing, and an async bus fault surfacing in
 *      Health(). This closes the gap with the akelc / waveshare fakes.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "gtest/gtest.h"

#include "xmdriver/hal/motor_factory.hpp"
#include "xmdriver/transport/can_interface.hpp"
#include "motor_vesc/vesc_frame.hpp"
#include "motor_vesc/vesc_motor.hpp"

using namespace xmotion;

namespace {

// Minimal in-memory CanInterface: records Open/Close ordering and the last
// frame sent, and exposes the receive/error callbacks so a test can inject
// status frames and async faults deterministically (no real bus, no sleeps).
class FakeCan : public CanInterface {
 public:
  bool Open() override {
    opened_ = true;
    return open_ok;
  }
  void Close() override {
    opened_ = false;
    close_op = ++op_seq_;
  }
  bool IsOpened() const override { return opened_; }
  void SetReceiveCallback(ReceiveCallback cb) override { rcv_ = std::move(cb); }
  void SetErrorCallback(ErrorCallback cb) override { err_ = std::move(cb); }
  TransportStatus SendFrame(const CanFrame& frame) override {
    ++sent_count;
    last_frame = frame;
    last_send_op = ++op_seq_;
    return send_status;
  }

  // Push a frame into the driver's receive path (as the I/O thread would).
  void Deliver(const CanFrame& frame) {
    if (rcv_) rcv_(frame);
  }
  // Raise an async bus fault on the driver's error path.
  void Fault(TransportStatus reason) {
    if (err_) err_(reason);
  }

  bool opened_ = false;
  bool open_ok = true;
  std::size_t sent_count = 0;
  CanFrame last_frame{};
  TransportStatus send_status = TransportStatus::kOk;
  int last_send_op = 0;  // op_seq_ at the last SendFrame
  int close_op = 0;      // op_seq_ at Close
  ReceiveCallback rcv_;
  ErrorCallback err_;

 private:
  int op_seq_ = 0;
};

// Decode the big-endian int32 payload the VESC command packets carry in
// data[0..3] (see utest_vesc_packet.cpp: SetRpm/SetCurrent scale into this).
int32_t DecodeI32(const CanFrame& f) {
  return static_cast<int32_t>((static_cast<uint32_t>(f.data[0]) << 24) |
                              (static_cast<uint32_t>(f.data[1]) << 16) |
                              (static_cast<uint32_t>(f.data[2]) << 8) |
                              static_cast<uint32_t>(f.data[3]));
}

// A valid Status1 frame for `id` carrying rpm=3314, current=0.2, duty=0.1
// (byte pattern reused from utest_vesc_packet.cpp).
CanFrame MakeStatus1Frame(uint8_t id) {
  CanFrame f;
  f.id = VescFrame::VescStatus1FrameId | id;
  f.extended = true;
  f.dlc = 8;
  f.data = {0x00, 0x00, 0x0c, 0xf2, 0x00, 0x02, 0x00, 0x64};
  return f;
}

// A VescMotor bound to `fake`, already connected.
std::unique_ptr<VescMotor> MakeConnected(std::shared_ptr<FakeCan> fake,
                                         uint8_t id = 3,
                                         double max_speed = 6000.0,
                                         double max_current = 20.0) {
  auto m = std::make_unique<VescMotor>(
      VescMotor::Config{"can0", id, max_speed, max_current}, fake);
  EXPECT_EQ(m->Connect(), hal::Status::kOk);
  EXPECT_TRUE(m->IsConnected());
  return m;
}

}  // namespace

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

// --- Behavioral coverage against an injected FakeCan transport ---

TEST(VescMotorFakeTest, ConnectFailsWhenTransportWontOpen) {
  auto fake = std::make_shared<FakeCan>();
  fake->open_ok = false;
  VescMotor m({"can0", 3, 6000.0, 20.0}, fake);
  EXPECT_EQ(m.Connect(), hal::Status::kIoError);
  EXPECT_FALSE(m.IsConnected());
}

TEST(VescMotorFakeTest, SetSpeedRejectsNonFinite) {
  auto fake = std::make_shared<FakeCan>();
  auto m = MakeConnected(fake);
  const std::size_t before = fake->sent_count;
  EXPECT_EQ(m->SetSpeed(hal::Rpm{std::nan("")}), hal::Status::kInvalidArgument);
  EXPECT_EQ(m->SetSpeed(hal::Rpm{INFINITY}), hal::Status::kInvalidArgument);
  EXPECT_EQ(m->SetSpeed(hal::Rpm{-INFINITY}), hal::Status::kInvalidArgument);
  // Rejected commands must not touch the bus.
  EXPECT_EQ(fake->sent_count, before);
}

TEST(VescMotorFakeTest, SetCurrentRejectsNonFinite) {
  auto fake = std::make_shared<FakeCan>();
  auto m = MakeConnected(fake);
  const std::size_t before = fake->sent_count;
  EXPECT_EQ(m->SetCurrent(hal::Ampere{std::nan("")}),
            hal::Status::kInvalidArgument);
  EXPECT_EQ(m->SetCurrent(hal::Ampere{INFINITY}),
            hal::Status::kInvalidArgument);
  EXPECT_EQ(fake->sent_count, before);
}

TEST(VescMotorFakeTest, SetSpeedInRangeSendsExactCommand) {
  auto fake = std::make_shared<FakeCan>();
  auto m = MakeConnected(fake, /*id=*/3, /*max_speed=*/6000.0);
  EXPECT_EQ(m->SetSpeed(hal::Rpm{1000.0}), hal::Status::kOk);
  EXPECT_EQ(fake->last_frame.id, VescFrame::VescRpmCmdFrameId | 3u);
  EXPECT_EQ(DecodeI32(fake->last_frame), 1000);
}

TEST(VescMotorFakeTest, SetSpeedClampsAndReportsOutOfRange) {
  auto fake = std::make_shared<FakeCan>();
  auto m = MakeConnected(fake, /*id=*/3, /*max_speed=*/6000.0);

  EXPECT_EQ(m->SetSpeed(hal::Rpm{50000.0}), hal::Status::kOutOfRange);
  EXPECT_EQ(fake->last_frame.id, VescFrame::VescRpmCmdFrameId | 3u);
  EXPECT_EQ(DecodeI32(fake->last_frame), 6000);  // clamped to +envelope

  EXPECT_EQ(m->SetSpeed(hal::Rpm{-50000.0}), hal::Status::kOutOfRange);
  EXPECT_EQ(DecodeI32(fake->last_frame), -6000);  // clamped to -envelope
}

TEST(VescMotorFakeTest, SetCurrentClampsAndReportsOutOfRange) {
  auto fake = std::make_shared<FakeCan>();
  auto m = MakeConnected(fake, /*id=*/3, /*max_speed=*/6000.0,
                         /*max_current=*/20.0);

  EXPECT_EQ(m->SetCurrent(hal::Ampere{100.0}), hal::Status::kOutOfRange);
  EXPECT_EQ(fake->last_frame.id, VescFrame::VescCurrentCmdFrameId | 3u);
  EXPECT_EQ(DecodeI32(fake->last_frame), 20000);  // 20 A * 1000 (mA scale)

  // Negative current is regen/braking and must reach the wire (clamped to the
  // -20 A envelope), not be floored to 0. Over-envelope is reported kOutOfRange
  // by the HAL layer AND the frame carries the clamped negative current.
  EXPECT_EQ(m->SetCurrent(hal::Ampere{-100.0}), hal::Status::kOutOfRange);
  EXPECT_EQ(DecodeI32(fake->last_frame), -20000);  // -20 A * 1000 (regen)
}

TEST(VescMotorFakeTest, ReportsTransportFailureOnSend) {
  auto fake = std::make_shared<FakeCan>();
  auto m = MakeConnected(fake);
  fake->send_status = TransportStatus::kIoError;
  EXPECT_EQ(m->SetSpeed(hal::Rpm{100.0}), hal::Status::kIoError);
}

TEST(VescMotorFakeTest, StaleReadUntilStatusFrameArrives) {
  auto fake = std::make_shared<FakeCan>();
  auto m = MakeConnected(fake, /*id=*/3);

  // No status frame yet: reads are stale, Health is degraded (not nominal).
  EXPECT_EQ(m->GetSpeed().status, hal::Status::kTimeout);
  EXPECT_EQ(m->GetCurrent().status, hal::Status::kTimeout);
  EXPECT_EQ(m->Health().state, hal::DeviceHealth::State::kDegraded);

  // A valid Status1 frame marks freshness -> reads succeed with real values.
  fake->Deliver(MakeStatus1Frame(3));
  auto s = m->GetSpeed();
  ASSERT_EQ(s.status, hal::Status::kOk);
  EXPECT_FLOAT_EQ(static_cast<float>(s.value.value()), 3314.0f);
  EXPECT_EQ(m->Health().state, hal::DeviceHealth::State::kOk);
}

TEST(VescMotorFakeTest, StatusFrameForOtherIdIsIgnored) {
  auto fake = std::make_shared<FakeCan>();
  auto m = MakeConnected(fake, /*id=*/3);
  // Frame addressed to a different VESC must not mark our freshness.
  fake->Deliver(MakeStatus1Frame(7));
  EXPECT_EQ(m->GetSpeed().status, hal::Status::kTimeout);
}

TEST(VescMotorFakeTest, StopSendsZeroCurrent) {
  auto fake = std::make_shared<FakeCan>();
  auto m = MakeConnected(fake, /*id=*/3);
  EXPECT_EQ(m->Stop(), hal::Status::kOk);
  EXPECT_EQ(fake->last_frame.id, VescFrame::VescCurrentCmdFrameId | 3u);
  EXPECT_EQ(DecodeI32(fake->last_frame), 0);  // zero torque -> coast
}

TEST(VescMotorFakeTest, DisconnectCommandsSafeStateThenCloses) {
  auto fake = std::make_shared<FakeCan>();
  auto m = MakeConnected(fake, /*id=*/3);
  m->Disconnect();

  EXPECT_FALSE(m->IsConnected());
  EXPECT_FALSE(fake->opened_);
  // The last thing sent was a zero-current command, and Close() happened after.
  EXPECT_EQ(fake->last_frame.id, VescFrame::VescCurrentCmdFrameId | 3u);
  EXPECT_EQ(DecodeI32(fake->last_frame), 0);
  EXPECT_GT(fake->close_op, fake->last_send_op);  // Stop() then Close()
}

TEST(VescMotorFakeTest, BusFaultSurfacesInHealth) {
  auto fake = std::make_shared<FakeCan>();
  auto m = MakeConnected(fake, /*id=*/3);

  // Make feedback fresh so the baseline health is kOk (not merely stale).
  fake->Deliver(MakeStatus1Frame(3));
  EXPECT_EQ(m->Health().state, hal::DeviceHealth::State::kOk);

  // An async bus fault latches and overrides freshness to kFault.
  fake->Fault(TransportStatus::kIoError);
  EXPECT_EQ(m->Health().state, hal::DeviceHealth::State::kFault);
}

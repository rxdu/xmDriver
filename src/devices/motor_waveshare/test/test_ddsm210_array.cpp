/*
 * test_ddsm210_array.cpp — Ddsm210Array (shared-bus coordinator) on the HAL.
 *
 * Exercised against a fake SerialInterface (no hardware): factory
 * registration with id-list parsing, id validation, per-id command frame
 * encoding, batch SetSpeeds as one bus round, per-id RX demux + freshness,
 * checksum-resync and unknown-id error paths, failsafe stop-all fan-out,
 * per-id + aggregate health, and actuator-group composition through the
 * per-id MemberRef capability views.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "gtest/gtest.h"

#include "xmbase/serialization/checksum.hpp"
#include "xmdriver/hal/actuator_group.hpp"
#include "xmdriver/hal/motor_factory.hpp"
#include "xmdriver/transport/serial_interface.hpp"

#include "motor_waveshare/ddsm_210_array.hpp"

using namespace xmotion;

namespace {

// In-memory SerialInterface: records every frame the driver sends and lets a
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
    frames.emplace_back(b, b + n);
    return send_status;
  }

  void Deliver(std::vector<uint8_t> bytes) {
    if (rcv_) rcv_(bytes.data(), bytes.size(), bytes.size());
  }

  bool opened_ = false;
  std::size_t sent_count = 0;
  std::vector<std::vector<uint8_t>> frames;
  TransportStatus send_status = TransportStatus::kOk;
  ReceiveCallback rcv_;
  ErrorCallback err_;
};

// Build a valid DDSM-210 speed-feedback (0x64) frame for the given id/fields.
std::vector<uint8_t> MakeSpeedFrame(uint8_t id, int16_t rpm_x10,
                                    int8_t temperature = 0) {
  std::vector<uint8_t> f(10, 0);
  f[0] = id;
  f[1] = 0x64;
  f[2] = static_cast<uint8_t>((rpm_x10 & 0xff00) >> 8);
  f[3] = static_cast<uint8_t>(rpm_x10 & 0x00ff);
  f[7] = static_cast<uint8_t>(temperature);
  f[9] = serialization::crc8_maxim(f.data(), 9);
  return f;
}

// Build a valid DDSM-210 odometry-feedback (0x74) frame.
std::vector<uint8_t> MakeOdomFrame(uint8_t id, int32_t encoder,
                                   uint16_t position, uint8_t error_code) {
  std::vector<uint8_t> f(10, 0);
  f[0] = id;
  f[1] = 0x74;
  f[2] = static_cast<uint8_t>((encoder >> 24) & 0xff);
  f[3] = static_cast<uint8_t>((encoder >> 16) & 0xff);
  f[4] = static_cast<uint8_t>((encoder >> 8) & 0xff);
  f[5] = static_cast<uint8_t>(encoder & 0xff);
  f[6] = static_cast<uint8_t>((position >> 8) & 0xff);
  f[7] = static_cast<uint8_t>(position & 0xff);
  f[8] = error_code;
  f[9] = serialization::crc8_maxim(f.data(), 9);
  return f;
}

// Decode the signed rpm*10 field of a sent 0x64 command frame.
int16_t SentRpmField(const std::vector<uint8_t>& frame) {
  return static_cast<int16_t>((static_cast<uint16_t>(frame[2]) << 8) |
                              static_cast<uint16_t>(frame[3]));
}

Ddsm210Array::Config TestConfig() {
  Ddsm210Array::Config cfg;
  cfg.bus = "/dev/ttyUSB0";
  cfg.ids = {1, 2, 3, 4};
  cfg.max_speed_rpm = 100.0;
  return cfg;
}

}  // namespace

TEST(Ddsm210ArrayTest, RegistersWithFactoryAndParsesIdList) {
  ASSERT_TRUE(RegisterDdsm210Array());
  EXPECT_TRUE(hal::MotorFactory::Instance().IsRegistered("ddsm210_array"));

  hal::MotorConfig cfg;
  cfg.type = "ddsm210_array";
  cfg.bus = "/dev/ttyUSB0";
  cfg.params["ids"] = "1, 2,3,4";
  auto m = hal::MotorFactory::Instance().Create(cfg);
  ASSERT_NE(m, nullptr);
  EXPECT_FALSE(m->IsConnected());
  EXPECT_EQ(m->Health().state, hal::DeviceHealth::State::kDisconnected);

  auto* array = dynamic_cast<Ddsm210Array*>(m.get());
  ASSERT_NE(array, nullptr);
  EXPECT_EQ(array->ids(), (std::vector<std::uint8_t>{1, 2, 3, 4}));
}

TEST(Ddsm210ArrayTest, FactoryRejectsMissingOrMalformedIds) {
  RegisterDdsm210Array();
  hal::MotorConfig cfg;
  cfg.type = "ddsm210_array";
  cfg.bus = "/dev/ttyUSB0";
  EXPECT_EQ(hal::MotorFactory::Instance().Create(cfg), nullptr);  // missing

  cfg.params["ids"] = "1,2,x";
  EXPECT_EQ(hal::MotorFactory::Instance().Create(cfg), nullptr);

  cfg.params["ids"] = "1,2,";
  EXPECT_EQ(hal::MotorFactory::Instance().Create(cfg), nullptr);

  cfg.params["ids"] = "300";
  EXPECT_EQ(hal::MotorFactory::Instance().Create(cfg), nullptr);
}

TEST(Ddsm210ArrayTest, ConnectRejectsBadIdLists) {
  {
    auto cfg = TestConfig();
    cfg.ids = {};
    Ddsm210Array m(cfg, std::make_shared<FakeSerial>());
    EXPECT_EQ(m.Connect(), hal::Status::kInvalidArgument);
  }
  {
    auto cfg = TestConfig();
    cfg.ids = {1, 2, 2};
    Ddsm210Array m(cfg, std::make_shared<FakeSerial>());
    EXPECT_EQ(m.Connect(), hal::Status::kInvalidArgument);
  }
  {
    auto cfg = TestConfig();
    cfg.ids = {1, 0xaa};  // broadcast id is reserved
    Ddsm210Array m(cfg, std::make_shared<FakeSerial>());
    EXPECT_EQ(m.Connect(), hal::Status::kInvalidArgument);
  }
}

TEST(Ddsm210ArrayTest, DisconnectedCommandsAndReadsRejected) {
  Ddsm210Array m(TestConfig());
  EXPECT_EQ(m.SetSpeed(1, hal::Rpm{50.0}), hal::Status::kNotConnected);
  EXPECT_EQ(m.SetPosition(1, hal::Radian{1.0}), hal::Status::kNotConnected);
  EXPECT_EQ(m.SetSpeeds({hal::Rpm{1}, hal::Rpm{2}, hal::Rpm{3}, hal::Rpm{4}}),
            hal::Status::kNotConnected);
  EXPECT_EQ(m.Stop(), hal::Status::kNotConnected);
  EXPECT_EQ(m.GetSpeed(1).status, hal::Status::kNotConnected);
  EXPECT_EQ(m.GetPosition(1).status, hal::Status::kNotConnected);
  EXPECT_EQ(m.GetState(1).status, hal::Status::kNotConnected);
}

TEST(Ddsm210ArrayTest, UnknownIdRejected) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  EXPECT_EQ(m.SetSpeed(9, hal::Rpm{10.0}), hal::Status::kInvalidArgument);
  EXPECT_EQ(m.SetPosition(9, hal::Radian{1.0}), hal::Status::kInvalidArgument);
  EXPECT_EQ(m.Stop(9), hal::Status::kInvalidArgument);
  EXPECT_EQ(m.GetSpeed(9).status, hal::Status::kInvalidArgument);
  EXPECT_EQ(m.GetPosition(9).status, hal::Status::kInvalidArgument);
  EXPECT_EQ(m.GetState(9).status, hal::Status::kInvalidArgument);
  EXPECT_EQ(m.Member(9), nullptr);
  EXPECT_EQ(fake->sent_count, 0u);  // nothing hit the wire
}

TEST(Ddsm210ArrayTest, PerIdCommandFrameEncoding) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  EXPECT_EQ(m.SetSpeed(3, hal::Rpm{50.0}), hal::Status::kOk);
  ASSERT_EQ(fake->frames.size(), 1u);
  const auto& f = fake->frames.back();
  ASSERT_EQ(f.size(), 10u);
  EXPECT_EQ(f[0], 3);        // addressed to id 3
  EXPECT_EQ(f[1], 0x64);     // drive command
  EXPECT_EQ(SentRpmField(f), 500);  // 50 RPM -> rpm*10
  EXPECT_EQ(f[9], serialization::crc8_maxim(f.data(), 9));  // valid checksum
}

TEST(Ddsm210ArrayTest, RejectsNonFiniteAndClampsToEnvelope) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  EXPECT_EQ(m.SetSpeed(1, hal::Rpm{std::nan("")}),
            hal::Status::kInvalidArgument);
  EXPECT_EQ(m.SetSpeed(1, hal::Rpm{INFINITY}), hal::Status::kInvalidArgument);
  EXPECT_EQ(fake->sent_count, 0u);  // rejected input never hits the wire

  EXPECT_EQ(m.SetSpeed(1, hal::Rpm{500.0}), hal::Status::kOutOfRange);
  EXPECT_EQ(SentRpmField(fake->frames.back()), 1000);  // clamped to 100 RPM

  EXPECT_EQ(m.SetPosition(1, hal::Radian{std::nan("")}),
            hal::Status::kInvalidArgument);
  EXPECT_EQ(m.SetPosition(1, hal::Radian{0.5}), hal::Status::kOk);
  EXPECT_EQ(m.SetPosition(1, hal::Radian{100.0}),
            hal::Status::kOutOfRange);  // >360 deg
}

TEST(Ddsm210ArrayTest, BatchSetSpeedsIsOneBusRoundInIdOrder) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  EXPECT_EQ(m.SetSpeeds({hal::Rpm{10.0}, hal::Rpm{-20.0}, hal::Rpm{30.0},
                         hal::Rpm{40.0}}),
            hal::Status::kOk);
  ASSERT_EQ(fake->frames.size(), 4u);
  const int16_t expect_rpm[] = {100, -200, 300, 400};
  for (std::size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(fake->frames[i][0], i + 1);  // registration order 1,2,3,4
    EXPECT_EQ(fake->frames[i][1], 0x64);
    EXPECT_EQ(SentRpmField(fake->frames[i]), expect_rpm[i]);
  }
}

TEST(Ddsm210ArrayTest, BatchSizeMismatchCommandsNothing) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  EXPECT_EQ(m.SetSpeeds({hal::Rpm{10.0}, hal::Rpm{20.0}}),
            hal::Status::kInvalidArgument);
  EXPECT_EQ(fake->sent_count, 0u);
}

TEST(Ddsm210ArrayTest, BatchCommandsEveryIdEvenAfterMemberFailure) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  // One NaN member fails; the other three must still be commanded and the
  // first failing status must be reported.
  EXPECT_EQ(m.SetSpeeds({hal::Rpm{10.0}, hal::Rpm{std::nan("")},
                         hal::Rpm{30.0}, hal::Rpm{40.0}}),
            hal::Status::kInvalidArgument);
  ASSERT_EQ(fake->frames.size(), 3u);
  EXPECT_EQ(fake->frames[0][0], 1);
  EXPECT_EQ(fake->frames[1][0], 3);
  EXPECT_EQ(fake->frames[2][0], 4);
}

TEST(Ddsm210ArrayTest, ReportsTransportFailureOnSend) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  fake->send_status = TransportStatus::kIoError;
  EXPECT_EQ(m.SetSpeed(1, hal::Rpm{50.0}), hal::Status::kIoError);
}

TEST(Ddsm210ArrayTest, PerIdFeedbackDemuxAndFreshness) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  // No feedback yet: every id reads stale.
  EXPECT_EQ(m.GetSpeed(1).status, hal::Status::kTimeout);
  EXPECT_EQ(m.GetSpeed(2).status, hal::Status::kTimeout);

  // One chunk carrying frames for ids 1 and 3 only.
  auto chunk = MakeSpeedFrame(1, 100);
  const auto f3 = MakeSpeedFrame(3, -250);
  chunk.insert(chunk.end(), f3.begin(), f3.end());
  fake->Deliver(chunk);

  auto s1 = m.GetSpeed(1);
  ASSERT_EQ(s1.status, hal::Status::kOk);
  EXPECT_DOUBLE_EQ(s1.value.value(), 10.0);
  auto s3 = m.GetSpeed(3);
  ASSERT_EQ(s3.status, hal::Status::kOk);
  EXPECT_DOUBLE_EQ(s3.value.value(), -25.0);
  // Ids without feedback stay stale — one member's data never masks another's.
  EXPECT_EQ(m.GetSpeed(2).status, hal::Status::kTimeout);
  EXPECT_EQ(m.GetSpeed(4).status, hal::Status::kTimeout);
}

TEST(Ddsm210ArrayTest, OdometryFeedbackServesPositionAndState) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  fake->Deliver(MakeSpeedFrame(2, 150, /*temperature=*/42));
  fake->Deliver(MakeOdomFrame(2, /*encoder=*/12345,
                              /*position=*/16384,  // ~180 deg
                              /*error_code=*/0));

  auto p = m.GetPosition(2);
  ASSERT_EQ(p.status, hal::Status::kOk);
  EXPECT_NEAR(p.value.value(), M_PI, 1e-3);
  EXPECT_EQ(m.GetEncoderCount(2), 12345);

  auto st = m.GetState(2);
  ASSERT_EQ(st.status, hal::Status::kOk);
  EXPECT_DOUBLE_EQ(st.value.speed_rpm, 15.0);
  EXPECT_DOUBLE_EQ(st.value.temperature_c, 42.0);
  EXPECT_EQ(st.value.encoder_count, 12345);
  EXPECT_EQ(st.value.error_code, 0);
}

TEST(Ddsm210ArrayTest, ResynchronizesAfterCorruptBytes) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  // Garbage (bad checksum) followed by a valid frame in the same chunk: the
  // scanner must discard byte-by-byte and still decode the valid frame.
  std::vector<uint8_t> chunk(7, 0x5a);
  const auto good = MakeSpeedFrame(1, 100);
  chunk.insert(chunk.end(), good.begin(), good.end());
  fake->Deliver(chunk);

  auto s = m.GetSpeed(1);
  ASSERT_EQ(s.status, hal::Status::kOk);
  EXPECT_DOUBLE_EQ(s.value.value(), 10.0);
}

TEST(Ddsm210ArrayTest, UnknownIdFrameSkippedWithoutDesync) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  // A well-formed frame from an unregistered id must be skipped whole, and
  // the following frame for a registered id must still decode.
  auto chunk = MakeSpeedFrame(7, 999);
  const auto good = MakeSpeedFrame(4, 300);
  chunk.insert(chunk.end(), good.begin(), good.end());
  fake->Deliver(chunk);

  auto s = m.GetSpeed(4);
  ASSERT_EQ(s.status, hal::Status::kOk);
  EXPECT_DOUBLE_EQ(s.value.value(), 30.0);
  EXPECT_EQ(m.GetSpeed(1).status, hal::Status::kTimeout);
}

TEST(Ddsm210ArrayTest, OdometryAndModeRequestsGoToTheRightId) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  m.RequestOdometryFeedbackAll();
  ASSERT_EQ(fake->frames.size(), 4u);
  for (std::size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(fake->frames[i][0], i + 1);
    EXPECT_EQ(fake->frames[i][1], 0x74);  // odometry request
  }

  fake->frames.clear();
  m.RequestModeFeedback(2);
  ASSERT_EQ(fake->frames.size(), 1u);
  EXPECT_EQ(fake->frames[0][0], 2);
  EXPECT_EQ(fake->frames[0][1], 0x75);  // mode request

  // Mode feedback (0x75) routes to the right id; others stay unknown.
  std::vector<uint8_t> f(10, 0);
  f[0] = 2;
  f[1] = 0x75;
  f[2] = 0x02;  // speed mode
  f[9] = serialization::crc8_maxim(f.data(), 9);
  fake->Deliver(f);
  EXPECT_EQ(m.GetMode(2), Ddsm210Array::Mode::kSpeed);
  EXPECT_EQ(m.GetMode(1), Ddsm210Array::Mode::kOpenLoop);  // default 0
  EXPECT_EQ(m.GetMode(9), Ddsm210Array::Mode::kUnknown);   // unknown id
}

TEST(Ddsm210ArrayTest, StopCommandsZeroSpeedToEveryId) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  EXPECT_EQ(m.Stop(), hal::Status::kOk);
  ASSERT_EQ(fake->frames.size(), 4u);
  for (std::size_t i = 0; i < 4; ++i) {
    EXPECT_EQ(fake->frames[i][0], i + 1);
    EXPECT_EQ(fake->frames[i][1], 0x64);
    EXPECT_EQ(SentRpmField(fake->frames[i]), 0);  // zero speed
  }
}

TEST(Ddsm210ArrayTest, StopFansOutEvenWhenSendsFail) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  fake->send_status = TransportStatus::kIoError;
  EXPECT_EQ(m.Stop(), hal::Status::kIoError);
  EXPECT_EQ(fake->sent_count, 4u);  // every id was still attempted
}

TEST(Ddsm210ArrayTest, DisconnectStopsAllThenClosesPort) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  m.Disconnect();
  EXPECT_EQ(fake->frames.size(), 4u);  // stop-all before close
  EXPECT_FALSE(fake->opened_);         // port closed after
  EXPECT_FALSE(m.IsConnected());
}

TEST(Ddsm210ArrayTest, PerIdAndAggregateHealth) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  // Connected but no data yet: every member (and the aggregate) is degraded.
  EXPECT_EQ(m.Health(1).state, hal::DeviceHealth::State::kDegraded);
  EXPECT_EQ(m.Health().state, hal::DeviceHealth::State::kDegraded);

  // Fresh, fault-free feedback for every id -> all Ok.
  for (uint8_t id = 1; id <= 4; ++id) {
    fake->Deliver(MakeSpeedFrame(id, 0));
    fake->Deliver(MakeOdomFrame(id, 0, 0, 0));
  }
  EXPECT_EQ(m.Health(1).state, hal::DeviceHealth::State::kOk);
  EXPECT_EQ(m.Health().state, hal::DeviceHealth::State::kOk);

  // A device fault on one id makes that member (and the aggregate) kFault
  // while the others stay kOk.
  fake->Deliver(MakeOdomFrame(3, 0, 0, /*error_code=*/0x10));  // over-temp bit
  EXPECT_EQ(m.Health(3).state, hal::DeviceHealth::State::kFault);
  EXPECT_EQ(m.Health(1).state, hal::DeviceHealth::State::kOk);
  EXPECT_EQ(m.Health().state, hal::DeviceHealth::State::kFault);

  EXPECT_EQ(m.Health(9).state, hal::DeviceHealth::State::kDisconnected);
}

TEST(Ddsm210ArrayTest, MemberRefsComposeIntoActuatorGroups) {
  auto fake = std::make_shared<FakeSerial>();
  Ddsm210Array m(TestConfig(), fake);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  auto* m1 = m.Member(1);
  auto* m2 = m.Member(2);
  ASSERT_NE(m1, nullptr);
  ASSERT_NE(m2, nullptr);

  // MemberRef forwards per-id verbs.
  EXPECT_EQ(m1->SetSpeed(hal::Rpm{25.0}), hal::Status::kOk);
  EXPECT_EQ(fake->frames.back()[0], 1);
  EXPECT_EQ(SentRpmField(fake->frames.back()), 250);
  fake->Deliver(MakeSpeedFrame(1, 250));
  auto s = m1->GetSpeed();
  ASSERT_EQ(s.status, hal::Status::kOk);
  EXPECT_DOUBLE_EQ(s.value.value(), 25.0);

  // MemberRefs are valid hal actuator-group members (capability + failsafe).
  hal::SpeedActuatorGroup left({{*m1}, {*m2}});
  fake->frames.clear();
  EXPECT_EQ(left.SetSpeed(hal::Rpm{12.0}), hal::Status::kOk);
  ASSERT_EQ(fake->frames.size(), 2u);
  EXPECT_EQ(fake->frames[0][0], 1);
  EXPECT_EQ(fake->frames[1][0], 2);
  EXPECT_EQ(SentRpmField(fake->frames[0]), 120);
  EXPECT_EQ(SentRpmField(fake->frames[1]), 120);
  EXPECT_EQ(left.Stop(), hal::Status::kOk);
}

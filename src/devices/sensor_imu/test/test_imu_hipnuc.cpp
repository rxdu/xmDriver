/*
 * test_imu_hipnuc.cpp — HiPNUC IMU on the new HAL, hardware-free properties.
 *
 * Verifies the read/lifecycle contract that does not need a real serial IMU:
 * a disconnected device reports kNotConnected and never a fake sample, an open
 * device with no fresh frame reports kTimeout (staleness), and connecting to a
 * missing device fails cleanly. (Frame decoding is covered on hardware.)
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <memory>

#include "gtest/gtest.h"

#include "sensor_imu/imu_hipnuc.hpp"
#include "xmdriver/transport/serial_interface.hpp"

using namespace xmotion;

namespace {
// Minimal in-memory serial that reports "open" but never delivers bytes, so a
// connected IMU has no fresh frame — exactly the staleness case.
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
  void SetReceiveCallback(ReceiveCallback) override {}
  void SetErrorCallback(ErrorCallback) override {}
  TransportStatus SendBytes(const uint8_t*, size_t) override {
    return TransportStatus::kOk;
  }

 private:
  bool opened_ = false;
};
}  // namespace

TEST(ImuHipnucTest, ReadWhileDisconnectedIsNotConnected) {
  ImuHipnuc imu({"/dev/xmnottyUSB", 115200});
  EXPECT_FALSE(imu.IsConnected());

  hal::Result<hal::ImuSample> r = imu.Read();
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.status, hal::Status::kNotConnected);

  EXPECT_EQ(imu.Health().state, hal::DeviceHealth::State::kDisconnected);
}

TEST(ImuHipnucTest, ConnectToMissingDeviceFailsCleanly) {
  // No such serial device in CI -> Connect must fail with a status, not crash,
  // and leave the IMU disconnected.
  ImuHipnuc imu({"/dev/xmnottyUSB", 115200});
  EXPECT_NE(imu.Connect(), hal::Status::kOk);
  EXPECT_FALSE(imu.IsConnected());

  // Still no data path: a read stays kNotConnected (never a stale/zero sample).
  hal::Result<hal::ImuSample> r = imu.Read();
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.status, hal::Status::kNotConnected);
}

TEST(ImuHipnucTest, ConnectedButNoFrameIsStaleTimeout) {
  // Open transport, but no frames ever arrive -> Read() must report kTimeout
  // (never a stale/zero sample dressed as kOk), and Health() is degraded.
  auto fake = std::make_shared<FakeSerial>();
  ImuHipnuc imu({"/dev/fake", 115200}, fake);
  ASSERT_EQ(imu.Connect(), hal::Status::kOk);
  EXPECT_TRUE(imu.IsConnected());

  hal::Result<hal::ImuSample> r = imu.Read();
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.status, hal::Status::kTimeout);
  EXPECT_EQ(imu.Health().state, hal::DeviceHealth::State::kDegraded);
}

TEST(ImuHipnucTest, SetSampleCallbackDoesNotFireWithoutFrames) {
  ImuHipnuc imu({"/dev/xmnottyUSB", 115200});
  bool fired = false;
  imu.SetSampleCallback([&](const hal::ImuSample&) { fired = true; });
  // No transport, no frames -> callback must never be invoked.
  EXPECT_FALSE(fired);
}

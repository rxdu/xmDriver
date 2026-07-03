/*
 * test_sbus_receiver.cpp — SbusReceiver on the new HAL, hardware-free.
 *
 * Drives the receiver through an injected fake SerialInterface: feeding a valid
 * SBUS frame yields a fresh kOk Read() and a frame callback; letting the link go
 * quiet past the watchdog window yields kTimeout, a degraded Health(), and a
 * failsafe callback (the safety-critical link-loss behaviour). Also covers the
 * divide-by-zero guard in RcReceiver::ScaleChannelValue.
 *
 * Copyright (c) 2024 Ruixiang Du (rdu)
 */

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "xmdriver/transport/serial_interface.hpp"
#include "input_sbus/sbus_receiver.hpp"

using namespace xmotion;

namespace {

// Minimal in-memory transport: captures the driver's callbacks and lets a test
// push bytes as if they arrived on the wire.
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
  void SetReceiveCallback(ReceiveCallback cb) override { rcv_cb_ = std::move(cb); }
  void SetErrorCallback(ErrorCallback cb) override { err_cb_ = std::move(cb); }
  TransportStatus SendBytes(const uint8_t*, size_t) override {
    return TransportStatus::kOk;
  }

  void Feed(std::vector<uint8_t> bytes) {
    if (rcv_cb_) rcv_cb_(bytes.data(), bytes.size(), bytes.size());
  }
  void FaultBus(TransportStatus st) {
    if (err_cb_) err_cb_(st);
  }

 private:
  std::atomic_bool opened_{false};
  ReceiveCallback rcv_cb_;
  ErrorCallback err_cb_;
};

// A full 25-byte SBUS frame: 0x0F header, 23 payload bytes, 0x00 tail. The flags
// byte (payload[22], i.e. frame[23]) carries frame_loss (0x20) / failsafe (0x10).
std::vector<uint8_t> MakeFrame(uint8_t flags = 0x00) {
  std::vector<uint8_t> f(25, 0x00);
  f[0] = 0x0F;
  f[23] = flags;
  f[24] = 0x00;
  return f;
}

}  // namespace

TEST(SbusReceiverTest, DisconnectedReadReportsNotConnected) {
  auto serial = std::make_shared<FakeSerial>();
  SbusReceiver rx(serial);
  EXPECT_FALSE(rx.IsConnected());
  auto r = rx.Read();
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.status, hal::Status::kNotConnected);
  EXPECT_EQ(rx.Health().state, hal::DeviceHealth::State::kDisconnected);
}

TEST(SbusReceiverTest, ValidFrameYieldsFreshReadAndCallback) {
  auto serial = std::make_shared<FakeSerial>();
  SbusReceiver rx(serial);

  std::atomic_int frames{0};
  hal::RcFrame last;
  rx.SetFrameCallback([&](const hal::RcFrame& f) {
    last = f;
    ++frames;
  });

  ASSERT_EQ(rx.Connect(), hal::Status::kOk);
  EXPECT_TRUE(rx.IsConnected());

  serial->Feed(MakeFrame());

  auto r = rx.Read();
  ASSERT_TRUE(r.ok());
  EXPECT_EQ(r.status, hal::Status::kOk);
  EXPECT_FALSE(r.value.failsafe);
  EXPECT_GE(frames.load(), 1);
  EXPECT_FALSE(last.failsafe);
  EXPECT_EQ(rx.Health().state, hal::DeviceHealth::State::kOk);

  rx.Disconnect();
}

TEST(SbusReceiverTest, LinkLossYieldsTimeoutAndFailsafeCallback) {
  auto serial = std::make_shared<FakeSerial>();
  SbusReceiver rx(serial);

  std::atomic_bool saw_failsafe{false};
  rx.SetFrameCallback([&](const hal::RcFrame& f) {
    if (f.failsafe) saw_failsafe = true;
  });

  ASSERT_EQ(rx.Connect(), hal::Status::kOk);
  serial->Feed(MakeFrame());  // one good frame, then silence

  // Wait past the 200 ms watchdog window (plus a tick of margin).
  std::this_thread::sleep_for(SbusReceiver::kLinkLossTimeout +
                              std::chrono::milliseconds(150));

  auto r = rx.Read();
  EXPECT_FALSE(r.ok());
  EXPECT_EQ(r.status, hal::Status::kTimeout);
  EXPECT_TRUE(saw_failsafe.load());
  EXPECT_NE(rx.Health().state, hal::DeviceHealth::State::kOk);

  rx.Disconnect();
}

TEST(SbusReceiverTest, ReceiverFrameLossFlagDegradesHealth) {
  auto serial = std::make_shared<FakeSerial>();
  SbusReceiver rx(serial);
  ASSERT_EQ(rx.Connect(), hal::Status::kOk);

  serial->Feed(MakeFrame(0x20));  // frame_loss set
  auto r = rx.Read();
  ASSERT_TRUE(r.ok());  // still a fresh frame, flag carried through
  EXPECT_TRUE(r.value.frame_loss);
  EXPECT_EQ(rx.Health().state, hal::DeviceHealth::State::kDegraded);

  rx.Disconnect();
}

TEST(SbusReceiverTest, ScaleChannelValueGuardsDivideByZero) {
  // Degenerate calibration (neutral == min == max) must not divide by zero.
  EXPECT_FLOAT_EQ(hal::RcReceiver::ScaleChannelValue(1000, 1000, 1000, 1000),
                  0.0f);
  // Normal scaling: midpoint of the upper half maps to +0.5.
  EXPECT_FLOAT_EQ(hal::RcReceiver::ScaleChannelValue(1500, 1000, 1000, 2000),
                  0.5f);
  // Lower half: midpoint maps to -0.5.
  EXPECT_FLOAT_EQ(hal::RcReceiver::ScaleChannelValue(500, 0, 1000, 2000),
                  -0.5f);
}

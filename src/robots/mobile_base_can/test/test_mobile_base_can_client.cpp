/*
 * MobileBaseCanClient (CAN realization of MobileBase): command encoding (+ counters), RX decode into
 * the freshness-gated snapshot, the not-connected / stale paths, and the
 * model-profile hook. Driven through a fake CAN bus — no hardware.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */
#include "mobile_base_can/mobile_base_can_client.hpp"

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "mobile_base_can/core_codec.hpp"
#include "mobile_base_can/profiles/swerve_v1.hpp"

using namespace xmotion;
using namespace xmotion::mobile_base;
namespace sw = xmotion::mobile_base::profiles::swerve_v1;

namespace {
// A CanInterface test double: captures sent frames, injects received ones.
class FakeCan : public CanInterface {
 public:
  bool Open() override { opened_ = true; return true; }
  void Close() override { opened_ = false; }
  bool IsOpened() const override { return opened_; }
  void SetReceiveCallback(ReceiveCallback cb) override { rx_ = std::move(cb); }
  void SetErrorCallback(ErrorCallback cb) override { err_ = std::move(cb); }
  TransportStatus SendFrame(const CanFrame& f) override {
    sent_.push_back(f);
    return TransportStatus::kOk;
  }
  void Deliver(const CanFrame& f) {
    if (rx_) rx_(f);
  }

  std::vector<CanFrame> sent_;
  ReceiveCallback rx_;
  ErrorCallback err_;
  bool opened_ = false;
};

std::shared_ptr<FakeCan> Connected(MobileBaseCanClient& base) {
  auto fake = std::make_shared<FakeCan>();
  EXPECT_EQ(base.Connect(fake), hal::Status::kOk);
  return fake;
}
}  // namespace

TEST(MobileBaseCanClient, NotConnectedIsSurfaced) {
  MobileBaseCanClient base({});
  EXPECT_EQ(base.SetTwist(0.5, 0, 0), hal::Status::kNotConnected);
  EXPECT_EQ(base.ReadStatus().status, hal::Status::kNotConnected);
}

TEST(MobileBaseCanClient, SetTwistEncodesAndCountsUp) {
  MobileBaseCanClient base({});
  auto fake = Connected(base);
  EXPECT_EQ(base.SetTwist(0.5, -0.2, 1.0), hal::Status::kOk);
  EXPECT_EQ(base.SetTwist(0.4, 0.0, -0.3), hal::Status::kOk);
  ASSERT_EQ(fake->sent_.size(), 2u);

  auto a = DecodeTwist(fake->sent_[0]);
  ASSERT_TRUE(a.has_value());
  EXPECT_NEAR(a->vx, 0.5, 1e-3);
  EXPECT_NEAR(a->wz, 1.0, 1e-3);

  auto b = DecodeTwist(fake->sent_[1]);
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(static_cast<int>(b->counter), static_cast<int>(a->counter) + 1);
}

TEST(MobileBaseCanClient, ClearEStopSendsKey) {
  MobileBaseCanClient base({});
  auto fake = Connected(base);
  EXPECT_EQ(base.ClearEStop(), hal::Status::kOk);
  ASSERT_EQ(fake->sent_.size(), 1u);
  auto c = DecodeEStop(fake->sent_[0]);
  ASSERT_TRUE(c.has_value());
  EXPECT_EQ(c->action, EStopAction::kClear);
  EXPECT_EQ(c->key, kEStopClearKey);
}

TEST(MobileBaseCanClient, RxDecodesIntoFreshSnapshot) {
  MobileBaseCanClient base({});
  auto fake = Connected(base);
  // Connected but no frame yet -> stale, not a false zero.
  EXPECT_EQ(base.ReadOdomTwist().status, hal::Status::kTimeout);

  fake->Deliver(Encode(OdomTwistState{1.25, -0.5, 0.8, 3}));
  const auto r = base.ReadOdomTwist();
  ASSERT_TRUE(r.ok());
  EXPECT_NEAR(r.value.vx, 1.25, 1e-3);
  EXPECT_NEAR(r.value.wz, 0.8, 1e-3);
}

TEST(MobileBaseCanClient, CorruptedRxIsNotAccepted) {
  MobileBaseCanClient base({});
  auto fake = Connected(base);
  CanFrame f = Encode(StatusState{});
  f.data[2] ^= 0x01;  // break CRC
  fake->Deliver(f);
  EXPECT_EQ(base.ReadStatus().status, hal::Status::kTimeout);  // never marked fresh
}

TEST(MobileBaseCanClient, ModelFrameHookReceivesExtensionFrames) {
  MobileBaseCanClient base({});
  int hits = 0;
  std::uint32_t seen_id = 0;
  base.SetModelFrameCallback([&](const CanFrame& f) {
    ++hits;
    seen_id = f.id;
  });
  auto fake = Connected(base);
  fake->Deliver(sw::Encode(sw::ModuleState{2, 0.6, 0.1, 0, 1}));  // 0x222
  EXPECT_EQ(hits, 1);
  EXPECT_EQ(seen_id, sw::ModuleId(2));
  // A core frame does NOT go to the model hook.
  fake->Deliver(Encode(StatusState{}));
  EXPECT_EQ(hits, 1);
}

// The client is usable purely through the abstract MobileBase — the type an
// upper layer (navigation) actually depends on. This is the whole point of the
// interface extraction: a base is swapped by swapping the implementation.
TEST(MobileBaseCanClient, UsableThroughAbstractInterface) {
  MobileBaseCanClient client({});
  auto fake = Connected(client);
  MobileBase& base = client;  // erase the concrete type
  EXPECT_EQ(base.SetTwist(0.3, 0.0, 0.1), hal::Status::kOk);
  ASSERT_EQ(fake->sent_.size(), 1u);
  EXPECT_TRUE(DecodeTwist(fake->sent_[0]).has_value());
  fake->Deliver(Encode(OdomTwistState{0.3, 0.0, 0.1, 1}));
  EXPECT_TRUE(base.ReadOdomTwist().ok());
}

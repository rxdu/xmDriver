/*
 * Commander-side driver: command encoding (+ advancing counters), RX decode into
 * the freshness-gated snapshot, the not-connected / stale paths, and the
 * model-profile hook. Driven through a fake CAN bus — no hardware.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */
#include "mobile_base/mobile_base_driver.hpp"

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "mobile_base/core_codec.hpp"
#include "mobile_base/profiles/swerve_v1.hpp"

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

std::shared_ptr<FakeCan> Connected(MobileBase& base) {
  auto fake = std::make_shared<FakeCan>();
  EXPECT_EQ(base.Connect(fake), hal::Status::kOk);
  return fake;
}
}  // namespace

TEST(MobileBaseDriver, NotConnectedIsSurfaced) {
  MobileBase base({});
  EXPECT_EQ(base.SetTwist(0.5, 0, 0), hal::Status::kNotConnected);
  EXPECT_EQ(base.ReadStatus().status, hal::Status::kNotConnected);
}

TEST(MobileBaseDriver, SetTwistEncodesAndCountsUp) {
  MobileBase base({});
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

TEST(MobileBaseDriver, ClearEStopSendsKey) {
  MobileBase base({});
  auto fake = Connected(base);
  EXPECT_EQ(base.ClearEStop(), hal::Status::kOk);
  ASSERT_EQ(fake->sent_.size(), 1u);
  auto c = DecodeEStop(fake->sent_[0]);
  ASSERT_TRUE(c.has_value());
  EXPECT_EQ(c->action, EStopAction::kClear);
  EXPECT_EQ(c->key, kEStopClearKey);
}

TEST(MobileBaseDriver, RxDecodesIntoFreshSnapshot) {
  MobileBase base({});
  auto fake = Connected(base);
  // Connected but no frame yet -> stale, not a false zero.
  EXPECT_EQ(base.ReadOdomTwist().status, hal::Status::kTimeout);

  fake->Deliver(Encode(OdomTwistState{1.25, -0.5, 0.8, 3}));
  const auto r = base.ReadOdomTwist();
  ASSERT_TRUE(r.ok());
  EXPECT_NEAR(r.value.vx, 1.25, 1e-3);
  EXPECT_NEAR(r.value.wz, 0.8, 1e-3);
}

TEST(MobileBaseDriver, CorruptedRxIsNotAccepted) {
  MobileBase base({});
  auto fake = Connected(base);
  CanFrame f = Encode(StatusState{});
  f.data[2] ^= 0x01;  // break CRC
  fake->Deliver(f);
  EXPECT_EQ(base.ReadStatus().status, hal::Status::kTimeout);  // never marked fresh
}

TEST(MobileBaseDriver, ModelFrameHookReceivesExtensionFrames) {
  MobileBase base({});
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

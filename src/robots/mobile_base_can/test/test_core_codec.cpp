/*
 * Core codec: round-trip every frame, then the E2E / validation reject paths and
 * encoder safety (NaN -> 0, saturation to wire range) (spec §6-§7).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */
#include "mobile_base_can/core_codec.hpp"

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

using namespace xmotion;
using namespace xmotion::mobile_base;

TEST(MobileBaseCoreCodec, CommandRoundTrips) {
  auto tw = DecodeTwist(Encode(TwistCommand{0.5, -0.25, 1.2, 7}));
  ASSERT_TRUE(tw.has_value());
  EXPECT_NEAR(tw->vx, 0.5, 1e-3);
  EXPECT_NEAR(tw->vy, -0.25, 1e-3);
  EXPECT_NEAR(tw->wz, 1.2, 1e-3);
  EXPECT_EQ(tw->counter, 7);

  auto md = DecodeMode(Encode(ModeCommand{ModeRequest::kAuto, 3}));
  ASSERT_TRUE(md.has_value());
  EXPECT_EQ(md->mode, ModeRequest::kAuto);

  auto es =
      DecodeEStop(Encode(EStopCommand{EStopAction::kClear, kEStopClearKey, 9}));
  ASSERT_TRUE(es.has_value());
  EXPECT_EQ(es->action, EStopAction::kClear);
  EXPECT_EQ(es->key, kEStopClearKey);

  auto hb = DecodeHeartbeat(Encode(HeartbeatCommand{kProtocolVersion, 1}));
  ASSERT_TRUE(hb.has_value());
  EXPECT_EQ(hb->version, kProtocolVersion);
}

TEST(MobileBaseCoreCodec, StateRoundTrips) {
  StatusState s{kProtocolVersion, ReportedMode::kAuto,
                static_cast<std::uint8_t>(kFlagCanFresh | kFlagRcLinkOk), 0x0042,
                5};
  auto st = DecodeStatus(Encode(s));
  ASSERT_TRUE(st.has_value());
  EXPECT_EQ(st->mode, ReportedMode::kAuto);
  EXPECT_EQ(st->flags, s.flags);
  EXPECT_EQ(st->faults, 0x0042);

  auto op = DecodeOdomPose(Encode(OdomPoseState{0.0123, -0.02, 0.05, 2}));
  ASSERT_TRUE(op.has_value());
  EXPECT_NEAR(op->dx, 0.0123, 1e-4);
  EXPECT_NEAR(op->dtheta, 0.05, 1e-4);

  CapabilitiesState c{kProtocolVersion, BaseType::kSwerve,
                      static_cast<std::uint8_t>(kDofVx | kDofVy | kDofWz),
                      4, 1, 1, 8};
  auto cap = DecodeCapabilities(Encode(c));
  ASSERT_TRUE(cap.has_value());
  EXPECT_EQ(cap->base_type, BaseType::kSwerve);
  EXPECT_EQ(cap->dof_mask, c.dof_mask);
  EXPECT_EQ(cap->wheel_count, 4);
  EXPECT_EQ(cap->model_profile_id, 1);

  auto lm = DecodeLimits(Encode(LimitsState{0.78, 0.78, 2.0, 0}));
  ASSERT_TRUE(lm.has_value());
  EXPECT_NEAR(lm->max_vx, 0.78, 1e-3);
  EXPECT_NEAR(lm->max_wz, 2.0, 1e-3);

  auto bt = DecodeBattery(Encode(BatteryState{24.6, -3.2, 87, 0}));
  ASSERT_TRUE(bt.has_value());
  EXPECT_NEAR(bt->voltage, 24.6, 1e-2);
  EXPECT_NEAR(bt->current, -3.2, 1e-2);
  EXPECT_EQ(bt->soc, 87);

  auto fl = DecodeFaults(Encode(FaultState{0xDEADBEEF, 6, 0}));
  ASSERT_TRUE(fl.has_value());
  EXPECT_EQ(fl->faults, 0xDEADBEEFu);
  EXPECT_EQ(fl->last_fault, 6);
}

TEST(MobileBaseCoreCodec, IntegrityRejectPaths) {
  {  // corrupted CRC
    CanFrame f = Encode(TwistCommand{0.5, 0, 0, 1});
    f.data[0] ^= 0x01;
    EXPECT_FALSE(DecodeTwist(f).has_value());
  }
  {  // masquerade: a valid TWIST reinterpreted under MODE's id (id is in the CRC)
    CanFrame f = Encode(TwistCommand{0.5, 0, 0, 1});
    f.id = kCmdMode;
    EXPECT_FALSE(DecodeMode(f).has_value());
  }
  {  // wrong DLC
    CanFrame f = Encode(TwistCommand{0.5, 0, 0, 1});
    f.dlc = 7;
    EXPECT_FALSE(DecodeTwist(f).has_value());
  }
  {  // e-stop clear with a wrong key is malformed (spec §6.3)
    CanFrame f = Encode(EStopCommand{EStopAction::kClear, 0x0000, 1});
    EXPECT_FALSE(DecodeEStop(f).has_value());
  }
  {  // mode value out of range, even with a valid CRC
    CanFrame f = MakeFrame(kCmdMode);
    f.data[0] = 5;
    StampE2e(f, 1);
    EXPECT_FALSE(DecodeMode(f).has_value());
  }
}

TEST(MobileBaseCoreCodec, EncoderSafety) {
  // NaN must never become motion: it encodes as zero.
  const double nan = std::numeric_limits<double>::quiet_NaN();
  auto z = DecodeTwist(Encode(TwistCommand{nan, 0, 0, 1}));
  ASSERT_TRUE(z.has_value());
  EXPECT_EQ(z->vx, 0.0);

  // Saturation clamps to the i16 wire range (1 mm/s LSB).
  auto sat = DecodeTwist(Encode(TwistCommand{1000.0, 0, 0, 1}));
  ASSERT_TRUE(sat.has_value());
  EXPECT_NEAR(sat->vx, 32.767, 1e-3);
}

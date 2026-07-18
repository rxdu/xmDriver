/*
 * Base-side command tracker: the integrity / version / freshness gates and the
 * e-stop latch (spec §4, §6, §9). Time is injected so no real clock is needed.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */
#include "mobile_base_can/command_tracker.hpp"

#include <gtest/gtest.h>

using namespace xmotion;
using namespace xmotion::mobile_base;

namespace {
using TP = CommandTracker::TimePoint;
const TP kT0{};  // epoch; add durations for later cycles
}  // namespace

TEST(MobileBaseCommandTracker, FreshTwistAccepted) {
  CommandTracker t;
  EXPECT_TRUE(t.OnFrame(Encode(TwistCommand{0.5, -0.2, 1.0, 1}), kT0));
  const auto s = t.Snapshot(kT0);
  EXPECT_TRUE(s.twist_fresh);
  EXPECT_NEAR(s.vx, 0.5, 1e-3);
  EXPECT_NEAR(s.vy, -0.2, 1e-3);
  EXPECT_NEAR(s.wz, 1.0, 1e-3);
}

TEST(MobileBaseCommandTracker, StaleTwistZeroed) {
  CommandTracker t;  // default 200 ms timeout
  t.OnFrame(Encode(TwistCommand{0.5, 0, 0, 1}), kT0);
  const auto s = t.Snapshot(kT0 + std::chrono::milliseconds(300));
  EXPECT_FALSE(s.twist_fresh);
  EXPECT_EQ(s.vx, 0.0);  // a stale command never drives
}

TEST(MobileBaseCommandTracker, ReplayRejected) {
  CommandTracker t;
  EXPECT_TRUE(t.OnFrame(Encode(TwistCommand{0.5, 0, 0, 5}), kT0));   // init
  EXPECT_FALSE(t.OnFrame(Encode(TwistCommand{9.9, 0, 0, 5}), kT0));  // replay
  EXPECT_TRUE(t.faults() & kFaultCmdIntegrity);
  // The replayed payload was not applied.
  EXPECT_NEAR(t.Snapshot(kT0).vx, 0.5, 1e-3);
}

TEST(MobileBaseCommandTracker, CorruptedCrcRejected) {
  CommandTracker t;
  CanFrame f = Encode(TwistCommand{0.5, 0, 0, 1});
  f.data[0] ^= 0x01;  // break the CRC
  EXPECT_FALSE(t.OnFrame(f, kT0));
  EXPECT_TRUE(t.faults() & kFaultCmdIntegrity);
  EXPECT_FALSE(t.Snapshot(kT0).twist_fresh);
}

TEST(MobileBaseCommandTracker, EStopLatchAndClear) {
  CommandTracker t;
  t.OnFrame(Encode(EStopCommand{EStopAction::kEngage, 0, 1}), kT0);
  EXPECT_TRUE(t.Snapshot(kT0).estop_engaged);
  // Clear requires the key — the decoder validates it before we ever see it.
  t.OnFrame(Encode(EStopCommand{EStopAction::kClear, kEStopClearKey, 2}), kT0);
  EXPECT_FALSE(t.Snapshot(kT0).estop_engaged);
}

TEST(MobileBaseCommandTracker, VersionMismatchGatesTwist) {
  CommandTracker t;  // expects kProtocolVersion (1)
  t.OnFrame(Encode(HeartbeatCommand{2, 1}), kT0);  // wrong version
  t.OnFrame(Encode(TwistCommand{0.5, 0, 0, 1}), kT0);  // integrity-ok, recent
  const auto s = t.Snapshot(kT0);
  EXPECT_FALSE(s.version_ok);
  EXPECT_FALSE(s.twist_fresh);  // untrusted version -> command not acted on
  EXPECT_TRUE(s.faults & kFaultProtoVersion);
}

TEST(MobileBaseCommandTracker, ModeTrackedAndNonCommandIgnored) {
  CommandTracker t;
  t.OnFrame(Encode(ModeCommand{ModeRequest::kAuto, 1}), kT0);
  EXPECT_EQ(t.Snapshot(kT0).mode, ModeRequest::kAuto);
  // A state frame is not a command — ignored, returns false, no fault.
  EXPECT_FALSE(t.OnFrame(Encode(StatusState{}), kT0));
  EXPECT_EQ(t.Snapshot(kT0).mode, ModeRequest::kAuto);
}

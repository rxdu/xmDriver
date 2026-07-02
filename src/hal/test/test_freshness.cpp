/*
 * test_freshness.cpp — FreshnessMonitor staleness transitions (ADR 0003).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "xmmu/hal/freshness.hpp"

#include <thread>

#include "gtest/gtest.h"

using namespace xmotion::hal;
using namespace std::chrono_literals;

TEST(FreshnessMonitor, NeverUpdatedIsNotFresh) {
  FreshnessMonitor f{50ms};
  EXPECT_FALSE(f.EverUpdated());
  EXPECT_FALSE(f.IsFresh());
}

TEST(FreshnessMonitor, MarkMakesFresh) {
  FreshnessMonitor f{200ms};
  f.Mark();
  EXPECT_TRUE(f.EverUpdated());
  EXPECT_TRUE(f.IsFresh());
}

TEST(FreshnessMonitor, GoesStaleAfterTimeout) {
  FreshnessMonitor f{20ms};
  f.Mark();
  EXPECT_TRUE(f.IsFresh());
  std::this_thread::sleep_for(60ms);
  EXPECT_FALSE(f.IsFresh());
  EXPECT_TRUE(f.EverUpdated());  // still "was updated", just stale
}

TEST(FreshnessMonitor, ResetClearsUpdated) {
  FreshnessMonitor f{200ms};
  f.Mark();
  f.Reset();
  EXPECT_FALSE(f.EverUpdated());
  EXPECT_FALSE(f.IsFresh());
}

TEST(HealthFromFreshness, MapsStates) {
  FreshnessMonitor f{20ms};
  EXPECT_EQ(HealthFromFreshness(false, f).state,
            DeviceHealth::State::kDisconnected);
  EXPECT_EQ(HealthFromFreshness(true, f).state,
            DeviceHealth::State::kDegraded);  // no data yet
  f.Mark();
  EXPECT_EQ(HealthFromFreshness(true, f).state, DeviceHealth::State::kOk);
  std::this_thread::sleep_for(60ms);
  EXPECT_EQ(HealthFromFreshness(true, f).state,
            DeviceHealth::State::kDegraded);  // stale
}

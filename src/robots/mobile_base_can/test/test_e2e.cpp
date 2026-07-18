/*
 * E2E integrity: CRC-8/AUTOSAR check value, stamp/verify, masquerade rejection,
 * and the alive-counter FSM (spec §4).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */
#include "mobile_base_can/e2e.hpp"

#include <gtest/gtest.h>

using namespace xmotion;
using namespace xmotion::mobile_base;

TEST(MobileBaseE2e, Crc8AutosarCheckValue) {
  const std::uint8_t s[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  EXPECT_EQ(Crc8Autosar(s, sizeof(s)), 0xDF);  // canonical check value
}

TEST(MobileBaseE2e, StampThenVerify) {
  CanFrame f;
  f.id = 0x123;
  f.data = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x00, 0x00};
  StampE2e(f, 42);
  EXPECT_EQ(f.data[kCounterByte], 42);
  EXPECT_TRUE(CrcValid(f));
}

TEST(MobileBaseE2e, CorruptionBreaksCrc) {
  CanFrame f;
  f.id = 0x123;
  f.data = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x00, 0x00};
  StampE2e(f, 42);

  CanFrame payload = f;
  payload.data[3] ^= 0x01;  // any payload bit flip
  EXPECT_FALSE(CrcValid(payload));

  CanFrame id = f;
  id.id ^= 0x01;  // masquerade: same bytes, different id (id is in the CRC)
  EXPECT_FALSE(CrcValid(id));
}

TEST(MobileBaseE2e, CounterFsm) {
  CounterCheck cc;  // default max_delta 15
  EXPECT_EQ(cc.Update(10), CounterCheck::Result::kInit);
  EXPECT_EQ(cc.Update(11), CounterCheck::Result::kInOrder);
  EXPECT_EQ(cc.Update(11), CounterCheck::Result::kStuck);    // replay
  EXPECT_EQ(cc.Update(12), CounterCheck::Result::kInOrder);  // recovers
  EXPECT_EQ(cc.Update(200), CounterCheck::Result::kGap);     // > max_delta
  EXPECT_EQ(cc.Update(201), CounterCheck::Result::kInOrder); // resynced
}

TEST(MobileBaseE2e, CounterWrapAround) {
  CounterCheck w;  // modular delta: 254 -> 1 is delta 3
  w.Update(254);
  EXPECT_EQ(w.Update(1), CounterCheck::Result::kInOrder);
}

TEST(MobileBaseE2e, AcceptedResults) {
  EXPECT_TRUE(CounterCheck::Accepted(CounterCheck::Result::kInit));
  EXPECT_TRUE(CounterCheck::Accepted(CounterCheck::Result::kInOrder));
  EXPECT_FALSE(CounterCheck::Accepted(CounterCheck::Result::kStuck));
  EXPECT_FALSE(CounterCheck::Accepted(CounterCheck::Result::kGap));
}

/*
 * utest_socketcan_frame.cpp — unit tests for the pure SocketCAN <-> CanFrame
 * conversion logic (async_port_detail::ToCanFrame / ToLinuxFrame). These cover
 * the EFF/RTR flag handling, id masking (CAN_EFF_MASK / CAN_SFF_MASK), and the
 * dlc clamp to 8 that were previously trapped in an anonymous namespace and
 * untested.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <cstring>

#include <gtest/gtest.h>

#include "async_port/detail/socketcan_frame.hpp"

using namespace xmotion;
using async_port_detail::ToCanFrame;
using async_port_detail::ToLinuxFrame;

TEST(SocketCanFrame, StandardIdRoundTrip) {
  CanFrame in;
  in.id = 0x123;
  in.extended = false;
  in.remote = false;
  in.dlc = 3;
  in.data = {0x11, 0x22, 0x33, 0, 0, 0, 0, 0};

  const struct can_frame raw = ToLinuxFrame(in);
  EXPECT_EQ(raw.can_id & CAN_EFF_FLAG, 0u);
  EXPECT_EQ(raw.can_id & CAN_RTR_FLAG, 0u);
  EXPECT_EQ(raw.can_id, 0x123u);
  EXPECT_EQ(raw.can_dlc, 3u);

  const CanFrame out = ToCanFrame(raw);
  EXPECT_EQ(out.id, 0x123u);
  EXPECT_FALSE(out.extended);
  EXPECT_FALSE(out.remote);
  EXPECT_EQ(out.dlc, 3u);
  EXPECT_EQ(out.data[0], 0x11);
  EXPECT_EQ(out.data[1], 0x22);
  EXPECT_EQ(out.data[2], 0x33);
}

TEST(SocketCanFrame, ExtendedIdSetsFlagAndPreserves29Bits) {
  CanFrame in;
  in.id = 0x1ABCDEF1;  // 29-bit id (fits in CAN_EFF_MASK = 0x1FFFFFFF)
  in.extended = true;
  in.dlc = 1;
  in.data[0] = 0x55;

  const struct can_frame raw = ToLinuxFrame(in);
  EXPECT_NE(raw.can_id & CAN_EFF_FLAG, 0u);
  EXPECT_EQ(raw.can_id & CAN_EFF_MASK, 0x1ABCDEF1u);

  const CanFrame out = ToCanFrame(raw);
  EXPECT_TRUE(out.extended);
  EXPECT_EQ(out.id, 0x1ABCDEF1u);  // 29-bit id preserved, flag stripped
  EXPECT_EQ(out.dlc, 1u);
  EXPECT_EQ(out.data[0], 0x55);
}

TEST(SocketCanFrame, IdMaskedTo11BitsWhenNotExtended) {
  CanFrame in;
  in.id = 0x7FF + 0x800;  // 0xFFF — bits above the 11-bit SFF range set
  in.extended = false;
  in.dlc = 0;

  const struct can_frame raw = ToLinuxFrame(in);
  EXPECT_EQ(raw.can_id, 0x7FFu);  // masked to CAN_SFF_MASK (11 bits)
  EXPECT_EQ(raw.can_id & CAN_EFF_FLAG, 0u);

  // And the RX direction masks a standard frame the same way.
  struct can_frame wire;
  std::memset(&wire, 0, sizeof(wire));
  wire.can_id = 0xFFF;  // no EFF flag => standard, must mask to 11 bits
  const CanFrame out = ToCanFrame(wire);
  EXPECT_FALSE(out.extended);
  EXPECT_EQ(out.id, 0x7FFu);
}

TEST(SocketCanFrame, RtrFlagBothDirections) {
  CanFrame in;
  in.id = 0x200;
  in.remote = true;
  in.extended = false;
  in.dlc = 4;  // dlc is meaningful for RTR (requested length) but data unused

  const struct can_frame raw = ToLinuxFrame(in);
  EXPECT_NE(raw.can_id & CAN_RTR_FLAG, 0u);

  const CanFrame out = ToCanFrame(raw);
  EXPECT_TRUE(out.remote);
  EXPECT_EQ(out.id, 0x200u);

  // RX direction: an RTR wire frame yields remote == true.
  struct can_frame wire;
  std::memset(&wire, 0, sizeof(wire));
  wire.can_id = 0x200 | CAN_RTR_FLAG;
  const CanFrame rx = ToCanFrame(wire);
  EXPECT_TRUE(rx.remote);
}

TEST(SocketCanFrame, DlcClampedToEightOnToLinuxFrame) {
  CanFrame in;
  in.id = 0x1;
  in.dlc = 15;  // out-of-range dlc must be clamped to 8
  in.data = {1, 2, 3, 4, 5, 6, 7, 8};

  const struct can_frame raw = ToLinuxFrame(in);
  EXPECT_EQ(raw.can_dlc, 8u);
  for (int i = 0; i < 8; ++i) EXPECT_EQ(raw.data[i], i + 1);
}

TEST(SocketCanFrame, DlcClampedToEightOnToCanFrame) {
  struct can_frame wire;
  std::memset(&wire, 0, sizeof(wire));
  wire.can_id = 0x1;
  wire.can_dlc = 12;  // malformed wire dlc > 8
  for (int i = 0; i < 8; ++i) wire.data[i] = 0xF0 + i;

  const CanFrame out = ToCanFrame(wire);
  EXPECT_EQ(out.dlc, 8u);
  for (int i = 0; i < 8; ++i) EXPECT_EQ(out.data[i], 0xF0 + i);
}

TEST(SocketCanFrame, DataBeyondDlcIgnored) {
  // ToLinuxFrame only copies the first dlc bytes; the rest stay zero.
  CanFrame in;
  in.id = 0x10;
  in.dlc = 2;
  in.data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};

  const struct can_frame raw = ToLinuxFrame(in);
  EXPECT_EQ(raw.can_dlc, 2u);
  EXPECT_EQ(raw.data[0], 0xAA);
  EXPECT_EQ(raw.data[1], 0xBB);
  for (int i = 2; i < 8; ++i) EXPECT_EQ(raw.data[i], 0u);  // beyond dlc ignored

  // ToCanFrame likewise ignores wire bytes past dlc (CanFrame.data is value-
  // initialized to zero).
  struct can_frame wire;
  std::memset(&wire, 0, sizeof(wire));
  wire.can_id = 0x10;
  wire.can_dlc = 2;
  for (int i = 0; i < 8; ++i) wire.data[i] = 0x90 + i;

  const CanFrame out = ToCanFrame(wire);
  EXPECT_EQ(out.dlc, 2u);
  EXPECT_EQ(out.data[0], 0x90);
  EXPECT_EQ(out.data[1], 0x91);
  for (int i = 2; i < 8; ++i) EXPECT_EQ(out.data[i], 0u);  // beyond dlc ignored
}

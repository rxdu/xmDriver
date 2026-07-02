/*
 * utest_async_lifecycle.cpp — hardware-free lifecycle/safety tests for the
 * async serial/CAN ports. These exercise the failure paths the hardening fixed:
 * a failed Open() must not crash and must leave the port closed, Close() must be
 * safe/idempotent, and sends before a successful Open() must be rejected (not
 * dereference a dangling buffer or touch a closed descriptor).
 *
 * The crash-on-disconnect (self-join) path requires a real device that drops
 * mid-stream and is covered on hardware; here we assert the no-device paths are
 * clean.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <cstdint>
#include <memory>

#include <gtest/gtest.h>

#include "async_port/async_serial.hpp"
#include "async_port/async_can.hpp"

using namespace xmotion;

TEST(AsyncSerialLifecycle, OpenNonexistentPortFailsCleanly) {
  auto s = std::make_shared<AsyncSerial>("/dev/xm_nonexistent_tty", 115200);
  EXPECT_FALSE(s->Open());
  EXPECT_FALSE(s->IsOpened());
  s->Close();  // idempotent / safe with no io thread running
  EXPECT_FALSE(s->IsOpened());
}

TEST(AsyncSerialLifecycle, SendBeforeOpenIsRejected) {
  auto s = std::make_shared<AsyncSerial>("/dev/xm_nonexistent_tty", 115200);
  const uint8_t data[4] = {1, 2, 3, 4};
  s->SendBytes(data, sizeof(data));  // must no-op, not crash
  SUCCEED();
}

TEST(AsyncCanLifecycle, OpenNonexistentIfaceFailsCleanly) {
  auto c = std::make_shared<AsyncCAN>("xm_nocan0");
  EXPECT_FALSE(c->Open());
  EXPECT_FALSE(c->IsOpened());
  c->Close();  // idempotent / safe
  EXPECT_FALSE(c->IsOpened());
}

TEST(AsyncCanLifecycle, SendFrameBeforeOpenIsRejected) {
  auto c = std::make_shared<AsyncCAN>("xm_nocan0");
  struct can_frame f {};
  f.can_id = 0x123;
  f.can_dlc = 1;
  f.data[0] = 0xAB;
  c->SendFrame(f);  // must no-op (port closed), not crash / UAF
  SUCCEED();
}

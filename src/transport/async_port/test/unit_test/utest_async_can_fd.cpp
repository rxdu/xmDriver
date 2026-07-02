/*
 * utest_async_can_fd.cpp — AsyncCAN RX / error-callback / backpressure exercised
 * over an injected stream fd (one end of a socketpair) via the test-only
 * AsyncCAN::OpenFd() seam. This runs the real asio read/write loop and the
 * bounded TX queue without a live PF_CAN interface. Synchronization uses
 * promises / condition_variables rather than sleeps; the backpressure loop is
 * bounded so it can never hang.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <sys/socket.h>
#include <unistd.h>
#include <linux/can.h>

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include "async_port/async_can.hpp"

using namespace xmotion;
using namespace std::chrono_literals;

namespace {

// Make a connected AF_UNIX stream pair with small send/receive buffers so the
// backpressure test fills the kernel buffer quickly and deterministically.
bool MakePair(int *a, int *b, int sndbuf = 2048) {
  int fds[2];
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) return false;
  // Shrink buffers on both ends (kernel doubles and clamps to a minimum).
  ::setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
  ::setsockopt(fds[0], SOL_SOCKET, SO_RCVBUF, &sndbuf, sizeof(sndbuf));
  ::setsockopt(fds[1], SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
  ::setsockopt(fds[1], SOL_SOCKET, SO_RCVBUF, &sndbuf, sizeof(sndbuf));
  *a = fds[0];
  *b = fds[1];
  return true;
}

}  // namespace

// (a) RX: a well-formed wire frame written to the peer is decoded and delivered
// to the receive callback as a first-party CanFrame.
TEST(AsyncCanFd, ReceiveDeliversConvertedFrame) {
  int can_fd = -1, peer = -1;
  ASSERT_TRUE(MakePair(&can_fd, &peer));

  auto can = std::make_shared<AsyncCAN>("test_can");
  std::promise<CanFrame> got;
  auto fut = got.get_future();
  std::once_flag once;
  can->SetReceiveCallback([&](const CanFrame &f) {
    std::call_once(once, [&] { got.set_value(f); });
  });

  ASSERT_TRUE(can->OpenFd(can_fd));
  ASSERT_TRUE(can->IsOpened());

  struct can_frame wire;
  std::memset(&wire, 0, sizeof(wire));
  wire.can_id = 0x1ABCDEF1 | CAN_EFF_FLAG;  // extended id
  wire.can_dlc = 3;
  wire.data[0] = 0xAA;
  wire.data[1] = 0xBB;
  wire.data[2] = 0xCC;
  ASSERT_EQ(::write(peer, &wire, sizeof(wire)),
            static_cast<ssize_t>(sizeof(wire)));

  ASSERT_EQ(fut.wait_for(2000ms), std::future_status::ready)
      << "receive callback never fired";
  const CanFrame f = fut.get();
  EXPECT_TRUE(f.extended);
  EXPECT_EQ(f.id, 0x1ABCDEF1u);
  EXPECT_EQ(f.dlc, 3u);
  EXPECT_EQ(f.data[0], 0xAA);
  EXPECT_EQ(f.data[1], 0xBB);
  EXPECT_EQ(f.data[2], 0xCC);

  can->Close();
  ::close(peer);
}

// (b) Error callback: closing the peer end makes the read loop observe EOF,
// which faults the port — the error callback fires and IsOpened() goes false.
TEST(AsyncCanFd, PeerCloseFiresErrorCallback) {
  int can_fd = -1, peer = -1;
  ASSERT_TRUE(MakePair(&can_fd, &peer));

  auto can = std::make_shared<AsyncCAN>("test_can");
  std::promise<TransportStatus> faulted;
  auto fut = faulted.get_future();
  std::once_flag once;
  can->SetErrorCallback([&](TransportStatus s) {
    std::call_once(once, [&] { faulted.set_value(s); });
  });

  ASSERT_TRUE(can->OpenFd(can_fd));
  ASSERT_TRUE(can->IsOpened());

  // Drop the peer: the pending async read completes with EOF -> fault.
  ::close(peer);

  ASSERT_EQ(fut.wait_for(2000ms), std::future_status::ready)
      << "error callback never fired on peer close";
  EXPECT_EQ(fut.get(), TransportStatus::kIoError);

  // The fault path marks the port closed.
  const auto deadline = std::chrono::steady_clock::now() + 2000ms;
  while (can->IsOpened() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::yield();
  }
  EXPECT_FALSE(can->IsOpened());

  can->Close();  // idempotent
}

// (c) Backpressure: with the peer never drained, writes stall once the kernel
// socket buffer fills; the bounded tx_queue_ then fills to kMaxTxQueue and
// SendFrame reports kQueueFull. The loop is bounded so it cannot hang.
TEST(AsyncCanFd, SendFrameReportsQueueFullUnderBackpressure) {
  int can_fd = -1, peer = -1;
  ASSERT_TRUE(MakePair(&can_fd, &peer));

  auto can = std::make_shared<AsyncCAN>("test_can");
  // No receive on the peer: the socket send buffer will fill and writes stall.
  ASSERT_TRUE(can->OpenFd(can_fd));
  ASSERT_TRUE(can->IsOpened());

  CanFrame f;
  f.id = 0x123;
  f.dlc = 8;
  f.data = {1, 2, 3, 4, 5, 6, 7, 8};

  // Upper bound: (small socket buffer / frame size) + in-flight + queue, with
  // generous slack. Far more than enough to reach kQueueFull; guarantees no hang.
  const int kMaxIterations = 200000;
  TransportStatus last = TransportStatus::kOk;
  int i = 0;
  for (; i < kMaxIterations; ++i) {
    last = can->SendFrame(f);
    if (last == TransportStatus::kQueueFull) break;
    ASSERT_EQ(last, TransportStatus::kOk)
        << "unexpected status before backpressure: " << ToString(last);
  }

  EXPECT_EQ(last, TransportStatus::kQueueFull)
      << "TX queue never reported full after " << i << " frames";
  EXPECT_LT(i, kMaxIterations) << "did not reach kQueueFull within bound";

  can->Close();
  ::close(peer);
}

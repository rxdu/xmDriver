/*
 * utest_async_loopback.cpp — AsyncSerial RX/TX exercised over a pseudo-terminal
 * (pty) so the real asio serial_port read/write path runs without hardware. A
 * pty master/slave pair loops bytes: writing to the master delivers to the slave
 * (which AsyncSerial owns) and vice-versa. Synchronization is via
 * promise/condition_variable on the receive callback — no fixed sleeps.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "async_port/async_serial.hpp"

using namespace xmotion;
using namespace std::chrono_literals;

namespace {

// Accumulates bytes delivered to the receive callback and lets a waiter block
// until at least `expected` bytes have arrived (deterministic, no sleep).
struct RxSink {
  std::mutex m;
  std::condition_variable cv;
  std::vector<uint8_t> bytes;

  void OnData(uint8_t *data, size_t /*bufsize*/, size_t len) {
    std::lock_guard<std::mutex> lk(m);
    bytes.insert(bytes.end(), data, data + len);
    cv.notify_all();
  }

  bool WaitFor(size_t expected, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lk(m);
    return cv.wait_for(lk, timeout,
                       [&] { return bytes.size() >= expected; });
  }
};

// Read exactly `n` bytes from `fd` with a bounded poll timeout. Returns the
// bytes actually read (may be short on timeout).
std::vector<uint8_t> ReadWithTimeout(int fd, size_t n,
                                     std::chrono::milliseconds timeout) {
  std::vector<uint8_t> out;
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (out.size() < n) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) break;
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
    struct pollfd pfd{fd, POLLIN, 0};
    const int pr = ::poll(&pfd, 1, static_cast<int>(remaining.count()));
    if (pr <= 0) break;  // timeout or error
    uint8_t buf[256];
    const ssize_t r = ::read(fd, buf, sizeof(buf));
    if (r <= 0) break;
    out.insert(out.end(), buf, buf + r);
  }
  return out;
}

// Opens a pty pair, returns master fd and the slave device path. slave fd is
// closed (AsyncSerial reopens the path itself); the pts persists while master
// stays open.
bool MakePty(int *master_fd, std::string *slave_path) {
  int master = -1, slave = -1;
  if (::openpty(&master, &slave, nullptr, nullptr, nullptr) != 0) return false;
  const char *name = ::ttyname(slave);
  if (name == nullptr) {
    ::close(master);
    ::close(slave);
    return false;
  }
  *slave_path = name;
  ::close(slave);
  *master_fd = master;
  return true;
}

}  // namespace

TEST(AsyncSerialLoopback, ReceiveDeliversBytesWrittenToMaster) {
  int master = -1;
  std::string slave_path;
  ASSERT_TRUE(MakePty(&master, &slave_path)) << "openpty failed";

  auto serial = std::make_shared<AsyncSerial>(slave_path, 115200);
  RxSink sink;
  serial->SetReceiveCallback(
      [&sink](uint8_t *d, size_t bs, size_t l) { sink.OnData(d, bs, l); });

  ASSERT_TRUE(serial->Open()) << "AsyncSerial could not open pty slave "
                              << slave_path;
  ASSERT_TRUE(serial->IsOpened());

  const uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02};
  const ssize_t w = ::write(master, payload, sizeof(payload));
  ASSERT_EQ(w, static_cast<ssize_t>(sizeof(payload)));

  EXPECT_TRUE(sink.WaitFor(sizeof(payload), 2000ms))
      << "receive callback did not deliver the expected bytes";
  {
    std::lock_guard<std::mutex> lk(sink.m);
    ASSERT_GE(sink.bytes.size(), sizeof(payload));
    for (size_t i = 0; i < sizeof(payload); ++i)
      EXPECT_EQ(sink.bytes[i], payload[i]) << "mismatch at byte " << i;
  }

  serial->Close();
  EXPECT_FALSE(serial->IsOpened());
  ::close(master);
}

TEST(AsyncSerialLoopback, SendBytesReachesMasterSide) {
  int master = -1;
  std::string slave_path;
  ASSERT_TRUE(MakePty(&master, &slave_path)) << "openpty failed";

  auto serial = std::make_shared<AsyncSerial>(slave_path, 115200);
  ASSERT_TRUE(serial->Open()) << "AsyncSerial could not open pty slave "
                              << slave_path;

  const uint8_t payload[] = {0x11, 0x22, 0x33, 0x44};
  EXPECT_EQ(serial->SendBytes(payload, sizeof(payload)), TransportStatus::kOk);

  const std::vector<uint8_t> got =
      ReadWithTimeout(master, sizeof(payload), 2000ms);
  ASSERT_EQ(got.size(), sizeof(payload))
      << "bytes sent on the slave did not appear on the master side";
  for (size_t i = 0; i < sizeof(payload); ++i)
    EXPECT_EQ(got[i], payload[i]) << "mismatch at byte " << i;

  serial->Close();
  ::close(master);
}

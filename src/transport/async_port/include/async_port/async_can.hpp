/*
 * async_can.hpp
 *
 * SocketCAN port on the shared asio io_context (ADR 0002 / ADR 0003). TX is a
 * bounded queue drained one write at a time on the I/O thread; SendFrame()
 * returns a TransportStatus so a caller sees backpressure (kQueueFull) or a dead
 * port (kNotOpen) instead of a silent void. The wire type at the interface is
 * the first-party CanFrame; the Linux `struct can_frame` stays internal.
 *
 * Copyright (c) 2020 Ruixiang Du (rdu)
 */

#ifndef ASYNC_CAN_HPP
#define ASYNC_CAN_HPP

#include <atomic>
#include <cstddef>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <functional>

#include "asio.hpp"
#include "asio/posix/basic_stream_descriptor.hpp"
#include <linux/can.h>

#include "xmdriver/transport/can_interface.hpp"

namespace xmotion {
class AsyncCAN : public std::enable_shared_from_this<AsyncCAN>,
                 public CanInterface {
 public:
  // Bound on the TX backlog. A stalled/bus-off link makes writes never complete;
  // without a bound the queue would grow without limit (a determinism hazard).
  static constexpr std::size_t kMaxTxQueue = 256;

  AsyncCAN(std::string can_port = "can0");
  ~AsyncCAN();

  AsyncCAN(const AsyncCAN &) = delete;
  AsyncCAN &operator=(const AsyncCAN &) = delete;

  // Public API
  bool Open() override;
  void Close() override;
  bool IsOpened() const override;

  void SetReceiveCallback(ReceiveCallback cb) override { rcv_cb_ = std::move(cb); }
  void SetErrorCallback(ErrorCallback cb) override { err_cb_ = std::move(cb); }

  TransportStatus SendFrame(const CanFrame &frame) override;

  // TEST-ONLY / INTERNAL SEAM — not part of the CanInterface contract.
  // Adopts an already-open stream fd (e.g. one end of a socketpair) in place of
  // a real PF_CAN socket: assigns it to socketcan_stream_, marks the port open,
  // and arms the read loop — mirroring Open() minus socket()/bind(). Lets the RX,
  // error-callback, and backpressure paths be exercised without CAN hardware.
  // Takes ownership of the fd (asio closes it on teardown). Do not use in
  // production code.
  bool OpenFd(int fd);

 private:
  std::string port_;
  std::atomic<bool> port_opened_{false};

  int can_fd_ = -1;
  asio::posix::basic_stream_descriptor<> socketcan_stream_;

  struct can_frame rcv_frame_;
  ReceiveCallback rcv_cb_ = nullptr;
  ErrorCallback err_cb_ = nullptr;

  // TX queue. Guarded by tx_mutex_ so SendFrame() (caller thread) can enqueue
  // and report backpressure synchronously; draining runs on the I/O thread.
  std::mutex tx_mutex_;
  std::deque<struct can_frame> tx_queue_;
  struct can_frame tx_frame_;  // in-flight frame, kept alive for the write
  bool tx_in_progress_ = false;

  void ReadFromPort();
  void StartWrite();      // I/O thread: kick the next queued write
  void HandleError(TransportStatus reason);  // I/O-thread teardown (no join)
};
}  // namespace xmotion

#endif /* ASYNC_CAN_HPP */

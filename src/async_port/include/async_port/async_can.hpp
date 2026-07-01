/*
 * async_can.hpp
 *
 * Created on: Sep 10, 2020 13:22
 * Description:
 *
 * Note: CAN TX is queued and serialized on the io thread — frames are sent in
 *  order, one async write in flight at a time. SendFrame() copies the frame, so
 *  the caller need not keep it alive, and all socket operations run on the io
 *  thread (no cross-thread use of the descriptor).
 *
 * Copyright (c) 2020 Ruixiang Du (rdu)
 */

#ifndef ASYNC_CAN_HPP
#define ASYNC_CAN_HPP

#include <linux/can.h>

#include <atomic>
#include <deque>
#include <memory>
#include <thread>
#include <functional>

#include "asio.hpp"
#include "asio/posix/basic_stream_descriptor.hpp"

#include "xmmu/transport/can_interface.hpp"

namespace xmotion {
class AsyncCAN : public std::enable_shared_from_this<AsyncCAN>,
                 public CanInterface {
 public:
  using ReceiveCallback = std::function<void(const struct can_frame *rx_frame)>;

 public:
  AsyncCAN(std::string can_port = "can0");
  ~AsyncCAN();

  // do not allow copy
  AsyncCAN(const AsyncCAN &) = delete;
  AsyncCAN &operator=(const AsyncCAN &) = delete;

  // Public API
  bool Open() override;
  void Close() override;
  bool IsOpened() const override;

  void SetReceiveCallback(ReceiveCallback cb) override { rcv_cb_ = cb; }

  void SendFrame(const struct can_frame &frame) override;

 private:
  std::string port_;
  std::atomic<bool> port_opened_{false};

#if ASIO_VERSION < 101200L
  asio::io_service io_context_;
#else
  asio::io_context io_context_;
#endif

  std::thread io_thread_;

  int can_fd_;
  asio::posix::basic_stream_descriptor<> socketcan_stream_;

  struct can_frame rcv_frame_;
  ReceiveCallback rcv_cb_ = nullptr;

  // TX queue — all accessed only on the io thread (SendFrame posts onto it).
  std::deque<struct can_frame> tx_queue_;
  struct can_frame tx_frame_;  // the in-flight frame, kept alive for the write
  bool tx_in_progress_ = false;

  void DefaultReceiveCallback(can_frame *rx_frame);
  void ReadFromPort(struct can_frame &rec_frame,
                    asio::posix::basic_stream_descriptor<> &stream);
  void StartWrite();   // io-thread only: kick the next queued write
  void HandleError();  // io-thread-safe teardown (does not join)
};
}  // namespace xmotion

#endif /* ASYNC_CAN_HPP */

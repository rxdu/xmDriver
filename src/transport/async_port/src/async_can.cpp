/*
 * async_can.cpp
 *
 * Created on: Sep 10, 2020 13:23
 * Description: see async_can.hpp.
 *
 * Copyright (c) 2020 Ruixiang Du (rdu)
 */

#include "async_port/async_can.hpp"

#include <net/if.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include <future>

#include "xmbase/logging/xlogger.hpp"

#include "async_port/io_service.hpp"
#include "async_port/detail/socketcan_frame.hpp"

namespace xmotion {
namespace {

using async_port_detail::ToCanFrame;
using async_port_detail::ToLinuxFrame;

}  // namespace

AsyncCAN::AsyncCAN(std::string can_port)
    : port_(std::move(can_port)),
      socketcan_stream_(IoService::Instance().context()) {}

AsyncCAN::~AsyncCAN() { Close(); }

bool AsyncCAN::Open() {
  const size_t iface_name_size = port_.size() + 1;
  if (iface_name_size > IFNAMSIZ) {
    XLOG_ERROR("CAN interface name too long: {}", port_);
    return false;
  }

  can_fd_ = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW);
  if (can_fd_ < 0) {
    XLOG_ERROR("Failed to open CAN socket on {}: {}", port_, strerror(errno));
    return false;
  }

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  memcpy(ifr.ifr_name, port_.c_str(), iface_name_size);
  if (ioctl(can_fd_, SIOCGIFINDEX, &ifr) < 0) {
    XLOG_ERROR("CAN interface {} not found: {}", port_, strerror(errno));
    ::close(can_fd_);
    can_fd_ = -1;
    return false;
  }

  struct sockaddr_can addr;
  memset(&addr, 0, sizeof(addr));
  addr.can_family = AF_CAN;
  addr.can_ifindex = ifr.ifr_ifindex;
  if (bind(can_fd_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) <
      0) {
    XLOG_ERROR("Failed to bind CAN socket on {}: {}", port_, strerror(errno));
    ::close(can_fd_);
    can_fd_ = -1;
    return false;
  }

  socketcan_stream_.assign(can_fd_);  // asio owns the fd from here
  port_opened_ = true;
  XLOG_INFO("CAN port opened: {}", port_);

  // Arm the read loop on the I/O thread.
  auto self = shared_from_this();
  asio::post(IoService::Instance().context(), [self] { self->ReadFromPort(); });
  return true;
}

bool AsyncCAN::OpenFd(int fd) {
  // TEST-ONLY / INTERNAL SEAM (see header). Adopt an already-open stream fd and
  // arm the read loop, skipping socket()/ioctl()/bind(). Runs the same read-loop
  // arming as Open() so the RX / error / backpressure paths behave identically.
  if (fd < 0) return false;
  can_fd_ = fd;
  socketcan_stream_.assign(can_fd_);  // asio owns the fd from here
  port_opened_ = true;
  auto self = shared_from_this();
  asio::post(IoService::Instance().context(), [self] { self->ReadFromPort(); });
  return true;
}

void AsyncCAN::Close() {
  // External / destructor teardown. Synchronize with the I/O thread: post the
  // socket teardown onto it (a descriptor is only touched from that thread) and
  // wait for it to run. Never called from a completion handler.
  if (!socketcan_stream_.is_open() && can_fd_ < 0) {
    port_opened_ = false;
    return;
  }
  std::promise<void> done;
  auto fut = done.get_future();
  auto self = shared_from_this();
  asio::post(IoService::Instance().context(), [self, &done] {
    self->HandleError(TransportStatus::kOk);  // kOk => intentional close
    done.set_value();
  });
  fut.wait();
}

void AsyncCAN::HandleError(TransportStatus reason) {
  // Runs on the I/O thread. Close the socket and mark closed; do not re-arm the
  // read loop and never join the shared I/O thread from within itself.
  std::error_code ec;
  if (socketcan_stream_.is_open()) {
    socketcan_stream_.cancel(ec);
    socketcan_stream_.close(ec);  // asio owns the fd post-assign
  } else if (can_fd_ >= 0) {
    ::close(can_fd_);
  }
  can_fd_ = -1;
  const bool was_open = port_opened_.exchange(false);
  {
    std::lock_guard<std::mutex> lk(tx_mutex_);
    tx_queue_.clear();
    tx_in_progress_ = false;
  }
  if (was_open && reason != TransportStatus::kOk) {
    XLOG_WARN("CAN port {} faulted: {}", port_, ToString(reason));
    if (err_cb_) err_cb_(reason);
  }
}

bool AsyncCAN::IsOpened() const { return port_opened_.load(); }

void AsyncCAN::ReadFromPort() {
  auto self = shared_from_this();
  socketcan_stream_.async_read_some(
      asio::buffer(&rcv_frame_, sizeof(rcv_frame_)),
      [self](asio::error_code error, size_t /*n*/) {
        if (error) {
          if (error != asio::error::operation_aborted)
            self->HandleError(TransportStatus::kIoError);
          return;
        }
        if (self->rcv_cb_) {
          const CanFrame f = ToCanFrame(self->rcv_frame_);
          self->rcv_cb_(f);
        }
        self->ReadFromPort();
      });
}

TransportStatus AsyncCAN::SendFrame(const CanFrame &frame) {
  if (!port_opened_.load()) return TransportStatus::kNotOpen;
  if (frame.dlc > 8) return TransportStatus::kInvalidArgument;

  const struct can_frame raw = ToLinuxFrame(frame);
  {
    std::lock_guard<std::mutex> lk(tx_mutex_);
    if (tx_queue_.size() >= kMaxTxQueue) {
      XLOG_WARN("CAN TX queue full on {} ({} frames), dropping frame", port_,
                kMaxTxQueue);
      return TransportStatus::kQueueFull;
    }
    tx_queue_.push_back(raw);
  }
  auto self = shared_from_this();
  asio::post(IoService::Instance().context(), [self] { self->StartWrite(); });
  return TransportStatus::kOk;
}

void AsyncCAN::StartWrite() {
  // I/O thread only. One write in flight; tx_frame_ owns the bytes for the write.
  {
    std::lock_guard<std::mutex> lk(tx_mutex_);
    if (tx_in_progress_ || tx_queue_.empty()) return;
    tx_in_progress_ = true;
    tx_frame_ = tx_queue_.front();
    tx_queue_.pop_front();
  }
  auto self = shared_from_this();
  socketcan_stream_.async_write_some(
      asio::buffer(&tx_frame_, sizeof(tx_frame_)),
      [self](asio::error_code error, size_t /*n*/) {
        {
          std::lock_guard<std::mutex> lk(self->tx_mutex_);
          self->tx_in_progress_ = false;
        }
        if (error) {
          if (error != asio::error::operation_aborted)
            self->HandleError(TransportStatus::kIoError);
          return;
        }
        self->StartWrite();  // drain the next queued frame, in order
      });
}

}  // namespace xmotion

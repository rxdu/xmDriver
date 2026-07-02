/*
 * async_can.cpp
 *
 * Created on: Sep 10, 2020 13:23
 * Description:
 *
 * Copyright (c) 2020 Ruixiang Du (rdu)
 */

#include "async_port/async_can.hpp"

#include <net/if.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/can.h>

#include <iostream>

namespace xmotion {
AsyncCAN::AsyncCAN(std::string can_port)
    : port_(can_port), socketcan_stream_(io_context_) {}

AsyncCAN::~AsyncCAN() { Close(); }

bool AsyncCAN::Open() {
  try {
    const size_t iface_name_size = strlen(port_.c_str()) + 1;
    if (iface_name_size > IFNAMSIZ) return false;

    can_fd_ = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW);
    if (can_fd_ < 0) return false;

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    memcpy(ifr.ifr_name, port_.c_str(), iface_name_size);

    const int ioctl_result = ioctl(can_fd_, SIOCGIFINDEX, &ifr);
    if (ioctl_result < 0) {
      Close();
      return false;
    }

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    const int bind_result =
        bind(can_fd_, (struct sockaddr *)&addr, sizeof(addr));
    if (bind_result < 0) {
      Close();
      return false;
    }

    port_opened_ = true;
    std::cout << "Start listening to port: " << port_ << std::endl;
  } catch (std::system_error &e) {
    port_opened_ = false;
    std::cout << e.what() << std::endl;
    return false;
  }

  // give some work to io_service to start async io chain
  socketcan_stream_.assign(can_fd_);

#if ASIO_VERSION < 101200L
  io_context_.post(std::bind(&AsyncCAN::ReadFromPort, this,
                             std::ref(rcv_frame_),
                             std::ref(socketcan_stream_)));
#else
  asio::post(io_context_,
             std::bind(&AsyncCAN::ReadFromPort, this, std::ref(rcv_frame_),
                       std::ref(socketcan_stream_)));
#endif

  // start io thread
  io_thread_ = std::thread([this]() { io_context_.run(); });

  return true;
}

void AsyncCAN::Close() {
  // External / destructor teardown: safe to join here (called from the owning
  // thread, never from a completion handler — those use HandleError()).
  io_context_.stop();
  if (io_thread_.joinable()) io_thread_.join();
  io_context_.reset();

  HandleError();  // close the socket / mark closed (idempotent)
}

void AsyncCAN::HandleError() {
  // Runs ON the io thread when invoked from a completion handler. Close the
  // socket and stop, but NEVER join the io thread from within itself — calling
  // Close() here was a self-join (terminate) on any CAN error (bus-off, iface
  // down). With the read loop not re-armed, the io_context drains and the io
  // thread exits; Close()/dtor then joins it.
  std::error_code ec;
  if (socketcan_stream_.is_open()) {
    // asio owns the fd after assign() — let it close (avoids a double close).
    socketcan_stream_.cancel(ec);
    socketcan_stream_.close(ec);
  } else if (can_fd_ >= 0) {
    // Open() failed before assign() — the raw fd is ours to close.
    ::close(can_fd_);
  }
  can_fd_ = -1;
  port_opened_ = false;
}

bool AsyncCAN::IsOpened() const { return port_opened_; }

void AsyncCAN::DefaultReceiveCallback(can_frame *rx_frame) {
  std::cout << std::hex << rx_frame->can_id << "  ";
  for (int i = 0; i < rx_frame->can_dlc; i++)
    std::cout << std::hex << int(rx_frame->data[i]) << " ";
  std::cout << std::dec << std::endl;
}

void AsyncCAN::ReadFromPort(struct can_frame &rec_frame,
                            asio::posix::basic_stream_descriptor<> &stream) {
  auto sthis = shared_from_this();
  stream.async_read_some(
      asio::buffer(&rec_frame, sizeof(rec_frame)),
      [sthis](asio::error_code error, size_t bytes_transferred) {
        if (error) {
          sthis->HandleError();
          return;
        }

        if (sthis->rcv_cb_ != nullptr)
          sthis->rcv_cb_(&sthis->rcv_frame_);
        else
          sthis->DefaultReceiveCallback(&sthis->rcv_frame_);

        sthis->ReadFromPort(std::ref(sthis->rcv_frame_),
                            std::ref(sthis->socketcan_stream_));
      });
}

void AsyncCAN::SendFrame(const struct can_frame &frame) {
  if (!port_opened_.load()) {
    std::cerr << "Failed to send, CAN port closed" << std::endl;
    return;
  }
  // Copy the frame and hand it to the io thread. This fixes two bugs in the old
  // direct async_write_some(&frame): (1) `frame` is the caller's object, which
  // the async write would reference after this returns -> use-after-free /
  // garbage on the bus; (2) issuing the write from the caller thread raced the
  // io thread's read on the same descriptor (asio requires serialized ops).
  auto sthis = shared_from_this();
  asio::post(io_context_, [sthis, frame]() {
    sthis->tx_queue_.push_back(frame);
    sthis->StartWrite();
  });
}

void AsyncCAN::StartWrite() {
  // io-thread only. One write in flight at a time; tx_frame_ owns the bytes for
  // the duration of the async write.
  if (tx_in_progress_ || tx_queue_.empty()) return;
  tx_in_progress_ = true;
  tx_frame_ = tx_queue_.front();
  tx_queue_.pop_front();

  auto sthis = shared_from_this();
  socketcan_stream_.async_write_some(
      asio::buffer(&tx_frame_, sizeof(tx_frame_)),
      [sthis](asio::error_code error, size_t /*bytes_transferred*/) {
        sthis->tx_in_progress_ = false;
        if (error) {
          std::cerr << "Failed to send CAN frame: " << error.message()
                    << std::endl;
          sthis->HandleError();
          return;
        }
        sthis->StartWrite();  // drain the next queued frame, in order
      });
}

}  // namespace xmotion
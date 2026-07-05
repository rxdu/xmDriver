/*
 * evdev_reader.cpp
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "input_hid/details/evdev_reader.hpp"

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <libevdev-1.0/libevdev/libevdev.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <utility>

#include "xmbase/telemetry/telemetry.hpp"

namespace xmotion::input_hid_detail {
namespace {
constexpr int kPollTimeoutMs = 200;  // bounds shutdown latency
}  // namespace

EvdevReader::EvdevReader(std::string device_path, EventFn on_event,
                         OnReadyFn on_ready, OnDisconnectFn on_disconnect)
    : device_path_(std::move(device_path)),
      on_event_(std::move(on_event)),
      on_ready_(std::move(on_ready)),
      on_disconnect_(std::move(on_disconnect)) {}

EvdevReader::~EvdevReader() { Close(); }

hal::Status EvdevReader::Open() {
  if (open_.load(std::memory_order_acquire)) return hal::Status::kOk;

  fd_ = ::open(device_path_.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd_ < 0) {
    XM_ERROR("EvdevReader: cannot open {}: {}", device_path_,
               std::strerror(errno));
    return hal::Status::kNotConnected;
  }

  const int rc = libevdev_new_from_fd(fd_, &dev_);
  if (rc < 0) {
    XM_ERROR("EvdevReader: libevdev init failed on {}: {}", device_path_,
               std::strerror(-rc));
    Teardown();
    return hal::Status::kIoError;
  }

  const char* name = libevdev_get_name(dev_);
  device_name_ = name ? name : device_path_;

  epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
  wake_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (epoll_fd_ < 0 || wake_fd_ < 0) {
    XM_ERROR("EvdevReader: epoll/eventfd setup failed for {}", device_path_);
    Teardown();
    return hal::Status::kIoError;
  }

  struct epoll_event dev_ev {};
  dev_ev.events = EPOLLIN;
  dev_ev.data.fd = fd_;
  struct epoll_event wake_ev {};
  wake_ev.events = EPOLLIN;
  wake_ev.data.fd = wake_fd_;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd_, &dev_ev) < 0 ||
      epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wake_fd_, &wake_ev) < 0) {
    XM_ERROR("EvdevReader: epoll_ctl ADD failed for {}", device_path_);
    Teardown();
    return hal::Status::kIoError;
  }

  // Hand the owner the device handle for capability queries (abs ranges) before
  // any event is dispatched.
  if (on_ready_) on_ready_(dev_);

  open_.store(true, std::memory_order_release);
  keep_running_.store(true, std::memory_order_release);
  thread_ = std::thread(&EvdevReader::RunLoop, this);

  XM_INFO("EvdevReader: opened {} ({})", device_path_, device_name_);
  return hal::Status::kOk;
}

void EvdevReader::Close() {
  keep_running_.store(false, std::memory_order_release);
  if (wake_fd_ >= 0) {
    const std::uint64_t one = 1;
    const ssize_t w = ::write(wake_fd_, &one, sizeof(one));
    (void)w;  // best-effort wakeup; the poll timeout is the backstop
  }
  if (thread_.joinable()) thread_.join();
  Teardown();
  open_.store(false, std::memory_order_release);
}

void EvdevReader::Teardown() {
  if (dev_ != nullptr) {
    libevdev_free(dev_);  // does NOT close the fd
    dev_ = nullptr;
  }
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  if (epoll_fd_ >= 0) {
    ::close(epoll_fd_);
    epoll_fd_ = -1;
  }
  if (wake_fd_ >= 0) {
    ::close(wake_fd_);
    wake_fd_ = -1;
  }
}

bool EvdevReader::DrainEvents() {
  int rc = LIBEVDEV_READ_STATUS_SUCCESS;
  do {
    struct input_event ev;
    rc = libevdev_next_event(dev_, LIBEVDEV_READ_FLAG_NORMAL, &ev);
    if (rc == LIBEVDEV_READ_STATUS_SYNC) {
      // SYN_DROPPED: the kernel dropped events. Replay the state delta in SYNC
      // mode until the queue is drained so our cached state re-converges.
      while (rc == LIBEVDEV_READ_STATUS_SYNC) {
        if (on_event_) on_event_(ev);
        rc = libevdev_next_event(dev_, LIBEVDEV_READ_FLAG_SYNC, &ev);
      }
    } else if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
      if (on_event_) on_event_(ev);
    }
  } while (rc == LIBEVDEV_READ_STATUS_SUCCESS ||
           rc == LIBEVDEV_READ_STATUS_SYNC);

  // rc < 0 here: -EAGAIN means fully drained; anything else is fatal (e.g.
  // -ENODEV when the device is unplugged).
  if (rc != -EAGAIN) {
    XM_WARN("EvdevReader: read error on {}: {}", device_path_,
              std::strerror(-rc));
    return false;
  }
  return true;
}

void EvdevReader::RunLoop() {
  struct epoll_event events[2];
  bool device_watched = true;
  while (keep_running_.load(std::memory_order_acquire)) {
    const int n = epoll_wait(epoll_fd_, events, 2, kPollTimeoutMs);
    if (n < 0) {
      if (errno == EINTR) continue;
      XM_ERROR("EvdevReader: epoll_wait failed on {}, errno {}", device_path_,
                 errno);
      break;
    }
    for (int i = 0; i < n; ++i) {
      if (events[i].data.fd == wake_fd_) return;  // shutdown requested
      if (device_watched && events[i].data.fd == fd_) {
        if (!DrainEvents()) {
          // Device is gone. Report it, stop watching the fd (so we don't spin),
          // and keep the thread parked on the wake fd until Close() joins it.
          open_.store(false, std::memory_order_release);
          epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd_, nullptr);
          device_watched = false;
          if (on_disconnect_) on_disconnect_();
        }
      }
    }
  }
}

}  // namespace xmotion::input_hid_detail

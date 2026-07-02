/*
 * hid_event_listener.cpp
 *
 * Created on 7/15/24 9:19 PM
 * Description: epoll-based HID fd-readiness reactor (replaces the libevent
 *   implementation — one fewer event-loop dependency; see ADR 0002).
 *
 * Copyright (c) 2024-2026 Ruixiang Du (rdu)
 */

#include "input_hid/hid_event_listener.hpp"

#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <stdexcept>

#include "xmsigma/logging/xlogger.hpp"

namespace xmotion {
namespace {
constexpr int kMaxEvents = 16;
constexpr int kPollTimeoutMs = 1000;  // bounds shutdown latency
}  // namespace

HidEventListener::HidEventListener() {
  epoll_fd_ = epoll_create1(0);
  if (epoll_fd_ < 0) {
    throw std::runtime_error("HidEventListener: failed to create epoll fd");
  }
}

HidEventListener::~HidEventListener() {
  running_ = false;
  if (event_thread_.joinable()) event_thread_.join();
  if (epoll_fd_ >= 0) {
    close(epoll_fd_);
    epoll_fd_ = -1;
  }
}

bool HidEventListener::AddHidHandler(HidInputInterface* handler) {
  const int fd = handler->GetFd();
  if (fd < 0) {
    XLOG_ERROR("HidEventListener: invalid file descriptor");
    return false;
  }
  // epoll_ctl is safe to call while another thread is in epoll_wait, so handlers
  // may be added before or after StartListening(). The handler pointer rides in
  // the event data for O(1) dispatch (ownership stays with the caller).
  struct epoll_event ev {};
  ev.events = EPOLLIN;
  ev.data.ptr = handler;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
    XLOG_ERROR("HidEventListener: epoll_ctl ADD failed for fd {}", fd);
    return false;
  }
  handlers_.push_back(handler);
  return true;
}

void HidEventListener::StartListening() {
  if (running_.exchange(true)) return;  // already listening
  event_thread_ = std::thread([this]() {
    struct epoll_event events[kMaxEvents];
    while (running_.load()) {
      const int n = epoll_wait(epoll_fd_, events, kMaxEvents, kPollTimeoutMs);
      if (n < 0) {
        if (errno == EINTR) continue;  // interrupted syscall -> retry
        XLOG_ERROR("HidEventListener: epoll_wait failed, errno {}", errno);
        break;
      }
      for (int i = 0; i < n; ++i) {
        auto* handler = static_cast<HidInputInterface*>(events[i].data.ptr);
        if (handler) handler->OnInputEvent();
      }
    }
  });
}
}  // namespace xmotion

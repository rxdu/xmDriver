/*
 * @file hid_event_listener.hpp
 * @date 7/15/24
 * @brief
 *
 * @copyright Copyright (c) 2024 Ruixiang Du (rdu)
 */

#ifndef XMOTION_HID_EVENT_LISTENER_HPP
#define XMOTION_HID_EVENT_LISTENER_HPP

#include <atomic>
#include <thread>
#include <vector>

#include "xmmu/hal/hid_handler_interface.hpp"

namespace xmotion {
// Watches the registered HID device fds for readiness and dispatches to each
// handler's OnInputEvent(). Backed by a POSIX epoll reactor — no external
// event-loop library (was libevent).
class HidEventListener {
 public:
  HidEventListener();
  ~HidEventListener();

  // do not allow copy
  HidEventListener(const HidEventListener&) = delete;
  HidEventListener& operator=(const HidEventListener&) = delete;

  // public methods
  bool AddHidHandler(HidInputInterface* handler);
  void StartListening();

 private:
  int epoll_fd_ = -1;
  std::atomic<bool> running_{false};
  std::vector<HidInputInterface*> handlers_;  // ownership stays with the caller
  std::thread event_thread_;
};
}  // namespace xmotion

#endif  // XMOTION_HID_EVENT_LISTENER_HPP

/*
 * evdev_reader.hpp
 *
 * Shared libevdev reader backing the joystick and keyboard HAL drivers. It owns
 * the device fd + libevdev handle and a single background thread that waits for
 * fd readiness (epoll) and drains input_events, so both devices share ONE
 * correct implementation of the open/drain/sync/teardown pattern (ADR 0003;
 * libevdev adopted over libevent per ADR 0002).
 *
 * Design notes / defects eliminated by construction:
 *   - The device fd is opened, read and closed ONLY on the reader thread, so a
 *     caller's Read()/Health() never touches the fd — no fd-after-disconnect
 *     race with the reader.
 *   - Dropped-event resync (SYN_DROPPED) is handled via LIBEVDEV_READ_FLAG_SYNC.
 *   - A hot-unplug surfaces as a read error (-ENODEV); the reader reports it via
 *     the disconnect hook and stops polling instead of spinning, so the owner's
 *     Health() can observe the disconnect. No inotify hot-plug (dropped cleanly).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#ifndef XMOTION_INPUT_HID_EVDEV_READER_HPP
#define XMOTION_INPUT_HID_EVDEV_READER_HPP

#include <atomic>
#include <functional>
#include <string>
#include <thread>

#include "xmmu/hal/status.hpp"

// Forward-declared so this header does not leak <libevdev.h> / <linux/input.h>
// into consumers; the implementation includes them.
struct libevdev;
struct input_event;

namespace xmotion::input_hid_detail {

class EvdevReader {
 public:
  // Invoked once during Open() with the libevdev handle so the owner can query
  // device capabilities (e.g. abs axis ranges) and the device name. May be empty.
  using OnReadyFn = std::function<void(libevdev*)>;
  // Invoked for every drained input_event, on the reader thread.
  using EventFn = std::function<void(const input_event&)>;
  // Invoked once when the device is detected as gone (read returned -ENODEV).
  using OnDisconnectFn = std::function<void()>;

  EvdevReader(std::string device_path, EventFn on_event,
              OnReadyFn on_ready = {}, OnDisconnectFn on_disconnect = {});
  ~EvdevReader();

  EvdevReader(const EvdevReader&) = delete;
  EvdevReader& operator=(const EvdevReader&) = delete;

  // Opens the device O_RDONLY|O_NONBLOCK, wraps it in libevdev, runs the
  // on-ready hook, and starts the reader thread. Returns a HAL Status.
  hal::Status Open();
  // Stops the reader thread and releases the device. Idempotent.
  void Close();

  bool IsOpen() const { return open_.load(std::memory_order_acquire); }
  const std::string& DevicePath() const { return device_path_; }
  // Device name reported by libevdev (valid after a successful Open()).
  const std::string& DeviceName() const { return device_name_; }

 private:
  void RunLoop();
  // Drain all pending events. Returns false if the device is gone (-ENODEV/other
  // fatal read error); true when fully drained (-EAGAIN).
  bool DrainEvents();
  void Teardown();  // free libevdev + close fds (does not touch the thread)

  const std::string device_path_;
  const EventFn on_event_;
  const OnReadyFn on_ready_;
  const OnDisconnectFn on_disconnect_;

  std::string device_name_;
  int fd_ = -1;
  int epoll_fd_ = -1;
  int wake_fd_ = -1;  // eventfd used to break epoll_wait on shutdown
  libevdev* dev_ = nullptr;
  std::thread thread_;
  std::atomic<bool> keep_running_{false};
  std::atomic<bool> open_{false};
};

}  // namespace xmotion::input_hid_detail

#endif  // XMOTION_INPUT_HID_EVDEV_READER_HPP

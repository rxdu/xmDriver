/*
 * evdev_keyboard.hpp
 *
 * Keyboard driver on the redesigned xmotion::hal interface (ADR 0001 + 0003),
 * backed by libevdev (ADR 0002). Replaces the old polling Keyboard and
 * event-driven KeyboardHandler with ONE implementation:
 *   - Device lifecycle (Connect/Disconnect/IsConnected/Health).
 *   - Read() -> Result<KeyboardState> (a bitset of pressed keys + stamp).
 *   - Event callback (SetKeyEventCallback).
 *   - Edge-triggered liveness: holds last key state while connected; a
 *     hot-unplug is observable via Health(), same contract as the joystick.
 *
 * Key codes come from the reused KeyboardMapping. All device-fd access happens
 * on the reader thread; Read()/Health() only read cached state.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#ifndef XMOTION_INPUT_HID_EVDEV_KEYBOARD_HPP
#define XMOTION_INPUT_HID_EVDEV_KEYBOARD_HPP

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>

#include "input_hid/details/keyboard_mapping.hpp"
#include "xmbase/telemetry/telemetry.hpp"
#include "xmdriver/hal/health_telemetry.hpp"
#include "xmdriver/hal/keyboard.hpp"

struct input_event;

namespace xmotion {
namespace input_hid_detail {
class EvdevReader;
}

class EvdevKeyboard final : public hal::Keyboard {
 public:
  struct Config {
    std::string device_path;  // e.g. "/dev/input/event3"
  };

  explicit EvdevKeyboard(Config cfg);
  explicit EvdevKeyboard(std::string device_path);
  ~EvdevKeyboard() override;

  EvdevKeyboard(const EvdevKeyboard&) = delete;
  EvdevKeyboard& operator=(const EvdevKeyboard&) = delete;

  // --- hal::Device ---
  hal::Status Connect() override;
  void Disconnect() override;
  bool IsConnected() const override;
  hal::DeviceHealth Health() const override;

  // --- hal::Keyboard ---
  hal::Result<hal::KeyboardState> Read() override;
  void SetKeyEventCallback(KeyEventCallback cb) override;

 private:
  void HandleEvent(const input_event& ev);  // reader thread

  Config cfg_;
  std::unique_ptr<input_hid_detail::EvdevReader> reader_;

  mutable std::mutex state_mtx_;
  hal::KeyboardState state_;

  mutable std::mutex cb_mtx_;
  KeyEventCallback key_cb_;

  std::atomic<bool> connected_{false};

  // Telemetry (observability only; handles registered once, no-op when no
  // backend is bound). Health transitions are reported where connected_
  // changes (Connect/Disconnect/hot-unplug), never per-sample.
  telemetry::EventSource src_ =
      telemetry::GetEventSource("driver.evdev_keyboard");
  telemetry::Counter hot_unplug_metric_ =
      telemetry::GetCounter("driver.evdev_keyboard.hot_unplug_count");
  hal::HealthReporter health_reporter_{"driver.evdev_keyboard"};
};

}  // namespace xmotion

#endif  // XMOTION_INPUT_HID_EVDEV_KEYBOARD_HPP

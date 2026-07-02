/*
 * evdev_joystick.hpp
 *
 * Joystick / gamepad driver on the redesigned xmotion::hal interface (ADR 0001 +
 * 0003), backed by libevdev (ADR 0002). Replaces the old polling Joystick and
 * event-driven JoystickHandler with ONE implementation:
 *   - Device lifecycle (Connect/Disconnect/IsConnected/Health).
 *   - Read() -> Result<JoystickState> with buttons + axes normalised to [-1, 1]
 *     from the device's own abs ranges (libevdev_get_abs_info), and a stamp.
 *   - Event callbacks (SetButtonCallback / SetAxisCallback).
 *   - Edge-triggered liveness: an idle joystick emits no events but is still
 *     live, so Read() holds the last state while connected; a hot-unplug is
 *     detected (libevdev -ENODEV) and surfaces as kNotConnected / Health().
 *
 * All device-fd access happens on the reader thread; Read()/Health() only touch
 * cached state, so there is no fd-after-disconnect race. Rumble / force-feedback
 * (an fd write from the caller thread) is intentionally dropped — it is outside
 * the HAL contract and would break the reader-only-fd invariant.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#ifndef XMOTION_INPUT_HID_EVDEV_JOYSTICK_HPP
#define XMOTION_INPUT_HID_EVDEV_JOYSTICK_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>

#include "xmmu/hal/joystick.hpp"

struct libevdev;
struct input_event;

namespace xmotion {
namespace input_hid_detail {
class EvdevReader;
}

class EvdevJoystick final : public hal::Joystick {
 public:
  struct Config {
    std::string device_path;  // e.g. "/dev/input/event8"
  };

  explicit EvdevJoystick(Config cfg);
  explicit EvdevJoystick(std::string device_path);
  ~EvdevJoystick() override;

  EvdevJoystick(const EvdevJoystick&) = delete;
  EvdevJoystick& operator=(const EvdevJoystick&) = delete;

  // --- hal::Device ---
  hal::Status Connect() override;
  void Disconnect() override;
  bool IsConnected() const override;
  hal::DeviceHealth Health() const override;

  // --- hal::Joystick ---
  std::string DeviceName() const override;
  hal::Result<hal::JoystickState> Read() override;
  void SetButtonCallback(ButtonCallback cb) override;
  void SetAxisCallback(AxisCallback cb) override;

 private:
  void OnDeviceReady(libevdev* dev);      // reader thread: read abs ranges
  void HandleEvent(const input_event& ev);  // reader thread: update state + emit

  Config cfg_;
  std::unique_ptr<input_hid_detail::EvdevReader> reader_;

  mutable std::mutex state_mtx_;
  hal::JoystickState state_;
  std::string device_name_;

  static constexpr std::size_t kNumAxes =
      static_cast<std::size_t>(hal::JsAxis::kLast);
  std::array<int, kNumAxes> axis_min_{};
  std::array<int, kNumAxes> axis_max_{};

  mutable std::mutex cb_mtx_;
  ButtonCallback button_cb_;
  AxisCallback axis_cb_;

  std::atomic<bool> connected_{false};
};

}  // namespace xmotion

#endif  // XMOTION_INPUT_HID_EVDEV_JOYSTICK_HPP

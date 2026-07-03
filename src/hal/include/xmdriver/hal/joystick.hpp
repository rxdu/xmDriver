/*
 * joystick.hpp
 *
 * Canonical HAL contract for a joystick / gamepad (ADR 0001 + 0003). Replaces
 * the legacy JoystickInterface (Open/Close vocab, raw fd, no freshness). A
 * hot-unplug is now observable: reads return Result, and Health() reflects
 * disconnect/staleness. Axes are normalised to [-1, 1] by the driver (from the
 * device's reported abs range), so consumers get a device-independent value.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <functional>
#include <string>

#include "xmdriver/hal/device.hpp"
#include "xmdriver/hal/result.hpp"

namespace xmotion::hal {

enum class JsButton : int {
  kTrigger = 0, kThumb, kThumb2, kTop1, kTop2, kPinkie, kBase, kBase2, kBase3,
  kBase4, kBase5, kBase6, kSouth, kEast, kNorth, kWest, kTL, kTR, kTL2, kTR2,
  kSelect, kStart, kMode, kThumbL, kThumbR, kLast
};

enum class JsAxis : int {
  kX = 0, kY, kZ, kRX, kRY, kRZ, kThrottle, kRudder, kWheel, kGas, kBrake,
  kHat0X, kHat0Y, kHat1X, kHat1Y, kLast
};

enum class ButtonEvent { kPress, kRelease };

struct JoystickState {
  std::array<bool, static_cast<std::size_t>(JsButton::kLast)> buttons{};
  std::array<float, static_cast<std::size_t>(JsAxis::kLast)> axes{};  // [-1,1]
  std::chrono::steady_clock::time_point stamp{};

  bool button(JsButton b) const {
    return buttons[static_cast<std::size_t>(b)];
  }
  float axis(JsAxis a) const { return axes[static_cast<std::size_t>(a)]; }
};

class Joystick : public Device {
 public:
  using ButtonCallback = std::function<void(JsButton, ButtonEvent)>;
  using AxisCallback = std::function<void(JsAxis, float)>;  // value in [-1,1]

  virtual std::string DeviceName() const = 0;

  // Latest state snapshot. Input devices are edge-triggered — a steady joystick
  // emits no events yet is perfectly live — so Read() returns kOk with the last
  // held state whenever connected, and kNotConnected once the device is gone (a
  // hot-unplug the driver detects). Liveness is connection, not event recency.
  virtual Result<JoystickState> Read() = 0;

  virtual void SetButtonCallback(ButtonCallback cb) = 0;
  virtual void SetAxisCallback(AxisCallback cb) = 0;
};

}  // namespace xmotion::hal

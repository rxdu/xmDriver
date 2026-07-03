/*
 * keyboard.hpp
 *
 * Canonical HAL contract for a keyboard input device (ADR 0001 + 0003).
 * Replaces the legacy KeyboardInterface (StartMonitoring vocab, no lifecycle
 * or freshness). Event-driven with a polled state snapshot; a disconnect is
 * observable via Device::Health().
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <bitset>
#include <chrono>
#include <cstddef>
#include <functional>

#include "xmdriver/hal/device.hpp"
#include "xmdriver/hal/result.hpp"

namespace xmotion::hal {

enum class KeyboardCode {
  kUnknown = 0,
  kA, kB, kC, kD, kE, kF, kG, kH, kI, kJ, kK, kL, kM, kN, kO, kP, kQ, kR, kS,
  kT, kU, kV, kW, kX, kY, kZ,
  kEsc, kNum1, kNum2, kNum3, kNum4, kNum5, kNum6, kNum7, kNum8, kNum9, kNum0,
  kMinus, kEqual, kBackspace, kTab, kEnter, kSpace, kCapsLock,
  kF1, kF2, kF3, kF4, kF5, kF6, kF7, kF8, kF9, kF10, kF11, kF12,
  kLeftCtrl, kRightCtrl, kLeftShift, kRightShift, kLeftAlt, kRightAlt,
  kLeft, kRight, kUp, kDown, kHome, kEnd, kPageUp, kPageDown,
  kLast
};

enum class KeyboardEvent { kPress, kRelease };

struct KeyboardState {
  std::bitset<static_cast<std::size_t>(KeyboardCode::kLast)> pressed;
  std::chrono::steady_clock::time_point stamp{};

  bool is_pressed(KeyboardCode c) const {
    return pressed.test(static_cast<std::size_t>(c));
  }
};

class Keyboard : public Device {
 public:
  using KeyEventCallback = std::function<void(KeyboardCode, KeyboardEvent)>;

  // Latest state snapshot. Edge-triggered like a joystick: returns kOk with the
  // last held key state whenever connected, kNotConnected once the device is
  // gone. Liveness is connection, not event recency.
  virtual Result<KeyboardState> Read() = 0;

  virtual void SetKeyEventCallback(KeyEventCallback cb) = 0;
};

}  // namespace xmotion::hal

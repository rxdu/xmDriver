/*
 * rc_receiver.hpp
 *
 * Canonical HAL contract for an RC receiver (SBUS et al.), ADR 0001 + 0003.
 * Generalises the input_sbus behaviour that was already the safety reference:
 * an RcFrame carries frame_loss / failsafe flags and a monotonic timestamp, and
 * a link-loss watchdog surfaces as kTimeout on Read() plus a kFault/kDegraded
 * Health(). RC link loss is safety-critical, so freshness is mandatory here.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <functional>

#include "xmdriver/hal/device.hpp"
#include "xmdriver/hal/result.hpp"

namespace xmotion::hal {

struct RcFrame {
  static constexpr std::size_t kMaxChannels = 18;  // SBUS: 16 proportional + 2
  std::array<float, kMaxChannels> channels{};      // raw channel values
  bool frame_loss = false;       // receiver reported a dropped frame
  bool failsafe = false;         // receiver/link in failsafe
  std::chrono::steady_clock::time_point stamp{};  // host receive time
};

class RcReceiver : public Device {
 public:
  using FrameCallback = std::function<void(const RcFrame&)>;

  // Latest frame. kNotConnected if down, kTimeout on link loss (no fresh frame
  // within the watchdog window), kOk with a live frame otherwise. A kOk frame
  // may still carry frame_loss/failsafe set by the receiver — check them.
  virtual Result<RcFrame> Read() = 0;

  // Push path: invoked for each received frame AND on watchdog-detected link
  // loss (with failsafe=true) so a consumer can react to loss, not just frames.
  virtual void SetFrameCallback(FrameCallback cb) = 0;

  // Scale a raw channel value to [-1, 1] about a neutral point. Guards against a
  // degenerate calibration (neutral == min or == max) instead of dividing by 0.
  static float ScaleChannelValue(float value, float min, float neutral,
                                 float max) {
    if (value < neutral) {
      const float span = neutral - min;
      return span > 0.0f ? (value - neutral) / span : 0.0f;
    }
    const float span = max - neutral;
    return span > 0.0f ? (value - neutral) / span : 0.0f;
  }
};

}  // namespace xmotion::hal

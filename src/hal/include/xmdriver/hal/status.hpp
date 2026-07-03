/*
 * status.hpp
 *
 * Uniform outcome code for every HAL operation. Replaces the old mix of bool /
 * void / throw / 0-sentinel returns: every implementation reports failure the
 * same way, which is what lets an upper layer treat all hardware uniformly.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

namespace xmotion::hal {

enum class Status {
  kOk = 0,
  kNotConnected,     // device not connected / not opened
  kTimeout,          // bus/device did not respond in time
  kIoError,          // transport-level failure (read/write/CRC)
  kUnsupported,      // capability not provided by this device
  kOutOfRange,       // argument outside the device's safe/valid range
  kInvalidArgument,  // malformed argument (NaN, wrong size, bad id)
  kDeviceFault,      // device reported an internal fault (over-temp, stall, ...)
};

inline const char* ToString(Status s) {
  switch (s) {
    case Status::kOk: return "ok";
    case Status::kNotConnected: return "not_connected";
    case Status::kTimeout: return "timeout";
    case Status::kIoError: return "io_error";
    case Status::kUnsupported: return "unsupported";
    case Status::kOutOfRange: return "out_of_range";
    case Status::kInvalidArgument: return "invalid_argument";
    case Status::kDeviceFault: return "device_fault";
  }
  return "unknown";
}

}  // namespace xmotion::hal

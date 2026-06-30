/*
 * device.hpp
 *
 * Uniform lifecycle + health for every HAL device. Transport parameters (port,
 * baud, CAN interface, bus id, limits) are supplied at CONSTRUCTION, not here —
 * so this interface is transport-agnostic and an upper layer never spells out
 * "serial with a baud rate". Connect() just brings the already-configured device
 * online.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <string>

#include "xmmu/hal/status.hpp"

namespace xmotion::hal {

struct DeviceHealth {
  enum class State {
    kDisconnected,  // not connected
    kOk,            // connected and nominal
    kDegraded,      // connected but impaired (e.g. dropped frames, warnings)
    kFault,         // connected but reporting a fault (over-temp, stall, ...)
  };
  State state = State::kDisconnected;
  std::string detail;  // human-readable context for logs/diagnostics

  bool ok() const { return state == State::kOk; }
};

// Base of every HAL device. Connection parameters are configured at
// construction; this interface only governs going online/offline and reporting
// health uniformly.
class Device {
 public:
  virtual ~Device() = default;

  virtual Status Connect() = 0;
  virtual void Disconnect() = 0;
  virtual bool IsConnected() const = 0;
  virtual DeviceHealth Health() const = 0;
};

}  // namespace xmotion::hal

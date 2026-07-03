/*
 * can_interface.hpp
 *
 * Created on 5/30/23 10:28 PM
 * Description: transport-level CAN port contract. Uses the first-party CanFrame
 * (no linux/can.h leak) and returns TransportStatus from sends so a caller sees
 * backpressure / not-open instead of a silent void (ADR 0003).
 *
 * Copyright (c) 2023 Ruixiang Du (rdu)
 */

#pragma once

#include <functional>

#include "xmdriver/transport/can_frame.hpp"
#include "xmdriver/transport/transport_status.hpp"

namespace xmotion {
class CanInterface {
 public:
  using ReceiveCallback = std::function<void(const CanFrame &rx_frame)>;
  // Invoked on the I/O thread when the port faults asynchronously (bus-off,
  // interface down, unplug). Lets a driver mark itself degraded/faulted rather
  // than discovering the dead port only on the next failed send.
  using ErrorCallback = std::function<void(TransportStatus)>;

 public:
  virtual ~CanInterface() = default;

  // Public API
  virtual bool Open() = 0;
  virtual void Close() = 0;
  virtual bool IsOpened() const = 0;

  virtual void SetReceiveCallback(ReceiveCallback cb) = 0;
  virtual void SetErrorCallback(ErrorCallback cb) = 0;
  virtual TransportStatus SendFrame(const CanFrame &frame) = 0;
};
}  // namespace xmotion

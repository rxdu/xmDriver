/*
 * serial_interface.hpp
 *
 * Created on 5/30/23 10:35 PM
 * Description:
 *
 * Copyright (c) 2023 Ruixiang Du (rdu)
 */

#pragma once

#include <cstdint>
#include <string>
#include <functional>

#include "xmdriver/transport/transport_status.hpp"

namespace xmotion {
class SerialInterface {
 public:
  using ReceiveCallback =
      std::function<void(uint8_t *data, const size_t bufsize, size_t len)>;
  // Invoked on the I/O thread when the port faults asynchronously (device
  // unplug, read error), so a driver can react instead of only learning on the
  // next failed send.
  using ErrorCallback = std::function<void(TransportStatus)>;

 public:
  virtual ~SerialInterface() = default;

  virtual void SetBaudRate(unsigned baudrate) = 0;
  virtual void SetHardwareFlowControl(bool enabled) = 0;

  virtual bool Open() = 0;
  virtual void Close() = 0;
  virtual bool IsOpened() const = 0;

  virtual void SetReceiveCallback(ReceiveCallback cb) = 0;
  virtual void SetErrorCallback(ErrorCallback cb) = 0;
  // Returns kOk if the bytes were queued/sent, kNotOpen / kQueueFull / kIoError
  // otherwise — never a silent void (ADR 0003).
  virtual TransportStatus SendBytes(const uint8_t *bytes, size_t length) = 0;
};
}  // namespace xmotion


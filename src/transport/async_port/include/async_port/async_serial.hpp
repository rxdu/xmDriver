/*
 * async_serial.hpp
 *
 * Created on: Sep 10, 2020 12:59
 * Description:
 *
 * Copyright (c) 2020 Ruixiang Du (rdu)
 */

#ifndef ASYNC_SERIAL_HPP
#define ASYNC_SERIAL_HPP

#include <cstdint>
#include <memory>
#include <array>
#include <atomic>
#include <mutex>
#include <functional>

#include "asio.hpp"

#include "async_port/ring_buffer.hpp"
#include "xmmu/transport/serial_interface.hpp"

namespace xmotion {
class AsyncSerial : public std::enable_shared_from_this<AsyncSerial>,
                    public SerialInterface {
 public:
  using ReceiveCallback =
      std::function<void(uint8_t *data, const size_t bufsize, size_t len)>;

  enum class Parity { kNone, kOdd, kEven };
  enum class StopBits { kOne, kTwo };

 public:
  AsyncSerial(const std::string &port_name, uint32_t baud_rate = 115200);
  ~AsyncSerial();

  // do not allow copy
  AsyncSerial(const AsyncSerial &) = delete;
  AsyncSerial &operator=(const AsyncSerial &) = delete;

  // Public API
  void SetBaudRate(unsigned baudrate) override;
  void SetHardwareFlowControl(bool enabled) override;
  void SetParity(Parity parity);
  void SetStopBits(StopBits stop_bits);

  bool Open() override;
  void Close() override;
  bool IsOpened() const override;

  bool ChangeBaudRate(unsigned baudrate);
  void SetReceiveCallback(ReceiveCallback cb) override { rcv_cb_ = std::move(cb); }
  void SetErrorCallback(ErrorCallback cb) override { err_cb_ = std::move(cb); }

  TransportStatus SendBytes(const uint8_t *bytes, size_t length) override;

 private:
  std::string port_;
  std::atomic<bool> port_opened_{false};

  // serial port — runs on the process-wide shared io_context (IoService).
  asio::serial_port serial_port_;
  uint32_t baud_rate_ = 115200;
  bool hwflow_ = false;
  ReceiveCallback rcv_cb_ = nullptr;
  ErrorCallback err_cb_ = nullptr;
  Parity parity_ = Parity::kNone;
  StopBits stop_bits_ = StopBits::kOne;

  // tx/rx buffering
  static constexpr uint32_t rxtx_buffer_size = 1024 * 8;
  std::array<uint8_t, rxtx_buffer_size> rx_buf_;
  uint8_t tx_buf_[rxtx_buffer_size];
  RingBuffer<uint8_t, rxtx_buffer_size> tx_rbuf_;
  std::recursive_mutex tx_mutex_;
  bool tx_in_progress_ = false;

  void ReadFromPort();
  void WriteToPort(bool check_if_busy);
  // I/O-thread teardown from a completion handler (no join): closes the port and
  // reports the fault via the error callback. Close() (external) synchronizes
  // with the I/O thread separately.
  void HandleError(TransportStatus reason);
};
}  // namespace xmotion

#endif /* ASYNC_SERIAL_HPP */

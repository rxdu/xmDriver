/*
 * async_serial.cpp
 *
 * Created on: Sep 10, 2020 13:00
 * Description:
 *
 * Copyright (c) 2020 Ruixiang Du (rdu)
 */

#include "async_port/async_serial.hpp"

#if defined(__linux__)
#include <linux/serial.h>
#include <sys/ioctl.h>
#endif

#include <cstring>
#include <future>

#include "xmbase/telemetry/telemetry.hpp"

#include "async_port/io_service.hpp"

namespace xmotion {
#if defined(__linux__)
// Defined in serial_baud_linux.cpp (isolated TU: <asm/termbits.h> cannot share
// this file with the glibc <termios.h> that asio includes).
bool SetSerialBaudTermios2(int fd, unsigned baud);
#endif

AsyncSerial::AsyncSerial(const std::string &port_name, uint32_t baud_rate)
    : port_(port_name),
      serial_port_(IoService::Instance().context()),
      baud_rate_(baud_rate) {}

AsyncSerial::~AsyncSerial() { Close(); }

void AsyncSerial::SetBaudRate(unsigned baudrate) { baud_rate_ = baudrate; }

bool AsyncSerial::ChangeBaudRate(unsigned baudrate) {
  int fd = serial_port_.native_handle();

  // Preferred: termios2/BOTHER — programs the literal (possibly non-standard,
  // e.g. SBUS's 100000) rate and works on devices with no exposed UART
  // baud_base, notably cdc_acm USB serial (/dev/ttyACM*).
  if (SetSerialBaudTermios2(fd, baudrate)) {
    XM_INFO_SRC(src_, "Changed baudrate on {} to {} (termios2)", port_,
                baudrate);
    return true;
  }

  // Fallback: legacy custom-divisor path for real 8250 UARTs / FTDI adapters
  // that expose a baud_base but may not honor termios2.
  struct serial_struct serial;
  if (ioctl(fd, TIOCGSERIAL, &serial) < 0) {
    XM_ERROR_SRC(src_,
                 "set baud {} on {}: termios2 rejected and TIOCGSERIAL failed: "
                 "{}",
                 baudrate, port_, strerror(errno));
    return false;
  }

  serial.flags &= ~ASYNC_SPD_MASK;  // Clear custom speed flags
  serial.flags |= ASYNC_SPD_CUST;   // Enable custom speed
  serial.custom_divisor = serial.baud_base / baudrate;

  if (serial.custom_divisor == 0) {
    XM_ERROR_SRC(src_,
                 "cannot set baud {} on {}: termios2 rejected and no valid "
                 "divisor (baud_base {})",
                 baudrate, port_, serial.baud_base);
    return false;
  }

  if (ioctl(fd, TIOCSSERIAL, &serial) < 0) {
    XM_ERROR_SRC(src_, "TIOCSSERIAL failed on {}: {}", port_, strerror(errno));
    return false;
  }

  XM_INFO_SRC(src_, "Changed baudrate on {} to {} (divisor {}, base {})",
              port_, baudrate, serial.custom_divisor, serial.baud_base);

  return true;
}

void AsyncSerial::SetHardwareFlowControl(bool enabled) { hwflow_ = enabled; }

void AsyncSerial::SetParity(Parity parity) { parity_ = parity; }

void AsyncSerial::SetStopBits(StopBits stop_bits) { stop_bits_ = stop_bits; }

bool AsyncSerial::Open() {
  using SPB = asio::serial_port_base;

  try {
    serial_port_.open(port_);

    // Set baudrate and 8N1 mode
    serial_port_.set_option(SPB::baud_rate(baud_rate_));
    serial_port_.set_option(SPB::character_size(8));
    if (parity_ == Parity::kNone) {
      serial_port_.set_option(SPB::parity(SPB::parity::none));
    } else if (parity_ == Parity::kOdd) {
      serial_port_.set_option(SPB::parity(SPB::parity::odd));
    } else if (parity_ == Parity::kEven) {
      serial_port_.set_option(SPB::parity(SPB::parity::even));
    }
    if (stop_bits_ == StopBits::kOne) {
      serial_port_.set_option(SPB::stop_bits(SPB::stop_bits::one));
    } else if (stop_bits_ == StopBits::kTwo) {
      serial_port_.set_option(SPB::stop_bits(SPB::stop_bits::two));
    }
    serial_port_.set_option(SPB::flow_control(
        (hwflow_) ? SPB::flow_control::hardware : SPB::flow_control::none));

#if defined(__linux__)
    // Enable low latency mode on Linux
    {
      int fd = serial_port_.native_handle();
      struct serial_struct ser_info;
      ioctl(fd, TIOCGSERIAL, &ser_info);
      ser_info.flags |= ASYNC_LOW_LATENCY;
      ioctl(fd, TIOCSSERIAL, &ser_info);
    }
#endif

    port_opened_ = true;
    XM_INFO_SRC(src_, "Serial port opened: {}@{}", port_, baud_rate_);
  } catch (std::system_error &e) {
    XM_ERROR_SRC(src_, "Failed to open serial port {}: {}", port_, e.what());
    return false;
  }

  // Arm the read loop on the shared I/O thread.
  auto self = shared_from_this();
  asio::post(IoService::Instance().context(), [self] { self->ReadFromPort(); });
  return true;
}

void AsyncSerial::Close() {
  // External / destructor teardown. The descriptor is only touched on the I/O
  // thread, so post the teardown there and wait. Never called from a completion
  // handler (those use HandleError()).
  if (!serial_port_.is_open()) {
    port_opened_ = false;
    return;
  }
  std::promise<void> done;
  auto fut = done.get_future();
  auto self = shared_from_this();
  asio::post(IoService::Instance().context(), [self, &done] {
    std::error_code ec;
    self->serial_port_.cancel(ec);
    self->serial_port_.close(ec);
    self->port_opened_ = false;
    done.set_value();
  });
  fut.wait();
}

void AsyncSerial::HandleError(TransportStatus reason) {
  // Runs on the I/O thread from a completion handler. Tear the port down but
  // never join the shared I/O thread from within itself. With the read loop not
  // re-armed and the port closed, no further work is queued for this port.
  std::error_code ec;
  serial_port_.cancel(ec);
  serial_port_.close(ec);
  const bool was_open = port_opened_.exchange(false);
  {
    std::lock_guard<std::recursive_mutex> lock(tx_mutex_);
    tx_rbuf_.Reset();
    tx_in_progress_ = false;
    tx_buffer_bytes_.Set(0.0);
  }
  if (was_open && reason != TransportStatus::kOk) {
    fault_metric_.Add();
    XM_WARN_SRC(src_, "Serial port {} faulted: {}", port_, ToString(reason));
    if (err_cb_) err_cb_(reason);
  }
}

bool AsyncSerial::IsOpened() const { return port_opened_.load(); }

void AsyncSerial::ReadFromPort() {
  auto sthis = shared_from_this();
  serial_port_.async_read_some(
      asio::buffer(rx_buf_),
      [sthis](asio::error_code error, size_t bytes_transferred) {
        if (error) {
          if (error != asio::error::operation_aborted)
            sthis->HandleError(TransportStatus::kIoError);
          return;
        }

        if (sthis->rcv_cb_ != nullptr) {
          sthis->rcv_cb_(sthis->rx_buf_.data(), sthis->rx_buf_.size(),
                         bytes_transferred);
        }
        sthis->ReadFromPort();
      });
}

void AsyncSerial::WriteToPort(bool check_if_busy) {
  // do nothing if an async tx has already been initiated
  if (check_if_busy && tx_in_progress_) return;

  std::lock_guard<std::recursive_mutex> lock(tx_mutex_);
  if (tx_rbuf_.IsEmpty()) return;

  auto sthis = shared_from_this();
  tx_in_progress_ = true;
  std::vector<uint8_t> data(tx_rbuf_.GetOccupiedSize());
  auto len = tx_rbuf_.Read(data, tx_rbuf_.GetOccupiedSize());
  tx_buffer_bytes_.Set(static_cast<double>(tx_rbuf_.GetOccupiedSize()));
  std::memcpy(tx_buf_, data.data(), len);
  serial_port_.async_write_some(
      asio::buffer(tx_buf_, len),
      [sthis](asio::error_code error, size_t /*bytes_transferred*/) {
        if (error) {
          if (error != asio::error::operation_aborted)
            sthis->HandleError(TransportStatus::kIoError);
          return;
        }
        std::lock_guard<std::recursive_mutex> lock(sthis->tx_mutex_);
        if (sthis->tx_rbuf_.IsEmpty()) {
          sthis->tx_in_progress_ = false;
          return;
        } else {
          sthis->WriteToPort(false);
        }
      });
}

TransportStatus AsyncSerial::SendBytes(const uint8_t *bytes, size_t length) {
  if (!IsOpened()) return TransportStatus::kNotOpen;
  if (length == 0) return TransportStatus::kOk;
  if (length >= rxtx_buffer_size) return TransportStatus::kInvalidArgument;

  std::lock_guard<std::recursive_mutex> lock(tx_mutex_);
  if (tx_rbuf_.GetFreeSize() < length) {
    // Bounded TX buffer full — report backpressure instead of throwing.
    tx_drop_metric_.Add();
    XM_WARN_SRC(src_,
                "Serial TX buffer full on {} ({} bytes free < {}), dropping",
                port_, tx_rbuf_.GetFreeSize(), length);
    return TransportStatus::kQueueFull;
  }
  std::vector<uint8_t> data(bytes, bytes + length);
  tx_rbuf_.Write(data, length);
  tx_buffer_bytes_.Set(static_cast<double>(tx_rbuf_.GetOccupiedSize()));
  auto self = shared_from_this();
  asio::post(IoService::Instance().context(),
             [self] { self->WriteToPort(true); });
  return TransportStatus::kOk;
}
}  // namespace xmotion

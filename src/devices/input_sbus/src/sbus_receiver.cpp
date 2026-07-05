/*
 * sbus_receiver.cpp
 *
 * Copyright (c) 2024 Ruixiang Du (rdu)
 */

#include "input_sbus/sbus_receiver.hpp"

#include "async_port/async_serial.hpp"

#include "xmbase/telemetry/telemetry.hpp"

namespace xmotion {
namespace {
// SBUS wire format: 100000 baud, 8 data bits, even parity, 2 stop bits.
constexpr unsigned kSbusBaud = 100000;
// Watchdog cadence: check the link a few times per window so a loss surfaces
// promptly without a busy loop.
constexpr auto kWatchdogTick = std::chrono::milliseconds(50);
}  // namespace

SbusReceiver::SbusReceiver(std::string port)
    : port_(std::move(port)), owns_transport_(true) {}

SbusReceiver::SbusReceiver(std::shared_ptr<SerialInterface> serial)
    : owns_transport_(false), serial_(std::move(serial)) {}

SbusReceiver::~SbusReceiver() { Disconnect(); }

hal::Status SbusReceiver::Connect() {
  if (connected_) return hal::Status::kOk;

  decoder_.SbusDecoderInit();  // never resume mid-frame across a reconnect
  freshness_.Reset();
  transport_faulted_ = false;

  auto on_bytes = [this](uint8_t* data, std::size_t bufsize, std::size_t len) {
    OnBytes(data, bufsize, len);
  };
  auto on_error = [this](TransportStatus st) {
    transport_faulted_ = true;
    transport_fault_metric_.Add();
    XM_ERROR_SRC(src_, "SBUS transport fault on {}: {}", port_,
                 static_cast<int>(st));
  };

  if (owns_transport_) {
    // Open at a standard baud, then switch to SBUS's non-standard 100000 via the
    // custom-divisor path (asio cannot set 100000 through termios directly).
    auto serial = std::make_shared<AsyncSerial>(port_);
    serial->SetParity(AsyncSerial::Parity::kEven);
    serial->SetStopBits(AsyncSerial::StopBits::kTwo);
    serial->SetReceiveCallback(on_bytes);
    serial->SetErrorCallback(on_error);
    if (!serial->Open()) {
      XM_ERROR_SRC(src_, "SBUS: failed to open serial port {}", port_);
      return hal::Status::kIoError;
    }
    if (!serial->ChangeBaudRate(kSbusBaud)) {
      XM_ERROR_SRC(src_, "SBUS: failed to set {} baud on {}", kSbusBaud, port_);
      serial->Close();
      return hal::Status::kIoError;
    }
    serial_ = std::move(serial);
  } else {
    if (!serial_) return hal::Status::kNotConnected;
    serial_->SetReceiveCallback(on_bytes);
    serial_->SetErrorCallback(on_error);
    if (!serial_->Open()) {
      XM_ERROR_SRC(src_, "SBUS: failed to open injected serial transport");
      return hal::Status::kIoError;
    }
  }

  connect_time_ = std::chrono::steady_clock::now();
  connected_ = true;
  keep_running_ = true;
  // Link-loss watchdog: emit failsafe while no fresh frame arrives so a consumer
  // stops acting on the last stick/kill state. Runs off the caller/RX path.
  watchdog_ = std::thread([this]() {
    while (keep_running_) {
      const auto now = std::chrono::steady_clock::now();
      const bool link_ok =
          freshness_.EverUpdated()
              ? freshness_.IsFresh()
              : (now - connect_time_ <= kLinkLossTimeout);
      if (!link_ok) EmitFailsafe();
      std::this_thread::sleep_for(kWatchdogTick);
    }
  });

  XM_INFO_SRC(src_, "SBUS receiver connected on {}", port_);
  return hal::Status::kOk;
}

void SbusReceiver::Disconnect() {
  keep_running_ = false;
  if (watchdog_.joinable()) watchdog_.join();
  if (serial_) serial_->Close();
  if (owns_transport_) serial_.reset();
  connected_ = false;
  freshness_.Reset();
  health_reporter_.Update(hal::DeviceHealth::State::kDisconnected);
}

bool SbusReceiver::IsConnected() const {
  return connected_ && serial_ && serial_->IsOpened();
}

hal::DeviceHealth SbusReceiver::Health() const {
  hal::DeviceHealth h = hal::HealthFromFreshness(IsConnected(), freshness_);
  if (IsConnected()) {
    if (transport_faulted_) {
      h = {hal::DeviceHealth::State::kFault, "transport fault"};
    } else if (h.state == hal::DeviceHealth::State::kOk) {
      // A fresh frame can still carry a receiver fault/loss flag — surface it.
      std::lock_guard<std::mutex> lock(frame_mtx_);
      if (last_frame_.failsafe)
        h = {hal::DeviceHealth::State::kFault, "receiver failsafe"};
      else if (last_frame_.frame_loss)
        h = {hal::DeviceHealth::State::kDegraded, "frame loss"};
    }
    // else: kDegraded "stale/no data" already means link loss.
    // Publish the frame age already tracked by the freshness monitor.
    if (freshness_.EverUpdated()) {
      data_age_ms_.Set(
          std::chrono::duration<double, std::milli>(freshness_.Age()).count());
    }
  }
  health_reporter_.Update(h);  // transitions only; no-op without a backend
  return h;
}

hal::Result<hal::RcFrame> SbusReceiver::Read() {
  if (!IsConnected())
    return hal::Result<hal::RcFrame>::Err(hal::Status::kNotConnected);
  if (!freshness_.IsFresh())
    return hal::Result<hal::RcFrame>::Err(hal::Status::kTimeout);
  std::lock_guard<std::mutex> lock(frame_mtx_);
  return hal::Result<hal::RcFrame>::Ok(last_frame_);
}

void SbusReceiver::SetFrameCallback(FrameCallback cb) {
  std::lock_guard<std::mutex> lock(cb_mtx_);
  callback_ = std::move(cb);
}

void SbusReceiver::OnBytes(uint8_t* data, std::size_t /*bufsize*/,
                           std::size_t len) {
  for (std::size_t i = 0; i < len; ++i) {
    SbusMessage msg;
    if (!decoder_.SbusDecodeMessage(data[i], &msg)) continue;

    hal::RcFrame frame;
    static_assert(SBUS_CHANNEL_NUMBER <= hal::RcFrame::kMaxChannels,
                  "SBUS channel count exceeds RcFrame capacity");
    for (int ch = 0; ch < SBUS_CHANNEL_NUMBER; ++ch)
      frame.channels[ch] = static_cast<float>(msg.channels[ch]);
    frame.frame_loss = msg.frame_loss;
    frame.failsafe = msg.fault_protection;  // receiver-reported failsafe
    frame.stamp = std::chrono::steady_clock::now();

    {
      std::lock_guard<std::mutex> lock(frame_mtx_);
      last_frame_ = frame;
    }
    transport_faulted_ = false;
    freshness_.Mark();  // link is live again
    FireCallback(frame);
  }
}

void SbusReceiver::EmitFailsafe() {
  failsafe_metric_.Add();  // one per watchdog-emitted failsafe frame
  hal::RcFrame frame;      // channels default to 0
  frame.frame_loss = true;
  frame.failsafe = true;
  frame.stamp = std::chrono::steady_clock::now();
  // Do NOT Mark() freshness: the link is down, so Read() must still report
  // kTimeout. The failsafe frame is a push-only notification.
  FireCallback(frame);
}

void SbusReceiver::FireCallback(const hal::RcFrame& frame) {
  std::lock_guard<std::mutex> lock(cb_mtx_);
  if (callback_) callback_(frame);
}
}  // namespace xmotion

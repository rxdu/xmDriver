/*
 * sbus_receiver.hpp
 *
 * SBUS RC receiver on the redesigned xmotion::hal interface (ADR 0001 + 0003).
 * This driver was already the safety reference: a 200 ms link-loss watchdog, a
 * receiver-reported frame_loss/failsafe surface, and a push callback that fires
 * on link loss as well as on frames. The migration keeps that behaviour and maps
 * it onto the canonical contract:
 *   - Connect()/Disconnect()/IsConnected()/Health() (was Open()/Close()/IsOpened);
 *   - Read() returns Result<RcFrame> — kNotConnected if down, kTimeout on link
 *     loss (no fresh frame within the watchdog window), kOk with a live frame;
 *   - SetFrameCallback() fires per received frame AND on watchdog link loss with
 *     failsafe=true (the old EmitFailsafe behaviour).
 *
 * Per ADR 0002 the bundled rpi_sbus library is dropped: framing is done by the
 * first-party SbusDecoder over the async_port AsyncSerial transport (SBUS is
 * 100000 baud, 8E2).
 *
 * Copyright (c) 2024 Ruixiang Du (rdu)
 */

#ifndef SBUS_RECEIVER_HPP
#define SBUS_RECEIVER_HPP

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "xmmu/hal/freshness.hpp"
#include "xmmu/hal/rc_receiver.hpp"
#include "xmmu/transport/serial_interface.hpp"

#include "input_sbus/sbus_decoder.hpp"

namespace xmotion {
class SbusReceiver : public hal::RcReceiver {
 public:
  // No valid SBUS frame within this window => the link is considered lost, which
  // surfaces as kTimeout on Read(), a degraded Health(), and a failsafe callback.
  // SBUS frames arrive every ~7-14 ms, so 200 ms is many missed frames.
  static constexpr auto kLinkLossTimeout = std::chrono::milliseconds(200);

  // Production: build the AsyncSerial transport for `port` on Connect().
  explicit SbusReceiver(std::string port);
  // Dependency injection (tests / custom transports): use the supplied serial
  // port as-is (already-configured framing); Connect() only wires callbacks and
  // opens it. Ownership is shared with the caller.
  explicit SbusReceiver(std::shared_ptr<SerialInterface> serial);
  ~SbusReceiver() override;

  SbusReceiver(const SbusReceiver&) = delete;
  SbusReceiver& operator=(const SbusReceiver&) = delete;

  // --- hal::Device ---
  hal::Status Connect() override;
  void Disconnect() override;
  bool IsConnected() const override;
  hal::DeviceHealth Health() const override;

  // --- hal::RcReceiver ---
  hal::Result<hal::RcFrame> Read() override;
  void SetFrameCallback(FrameCallback cb) override;

 private:
  // Fed each received byte on the transport RX thread; publishes a frame when a
  // full SBUS packet is decoded.
  void OnBytes(uint8_t* data, std::size_t bufsize, std::size_t len);
  // Fired repeatedly by the watchdog while the link is down.
  void EmitFailsafe();
  void FireCallback(const hal::RcFrame& frame);

  const std::string port_;
  const bool owns_transport_;  // false when a transport was injected

  std::shared_ptr<SerialInterface> serial_;
  SbusDecoder decoder_;

  // Freshness drives Read()/Health(); Mark()ed on every decoded frame.
  hal::FreshnessMonitor freshness_{kLinkLossTimeout};

  mutable std::mutex frame_mtx_;  // guards last_frame_
  hal::RcFrame last_frame_;

  std::mutex cb_mtx_;  // guards callback_ (set from any thread, used on RX thread)
  FrameCallback callback_;

  std::atomic_bool connected_{false};
  std::atomic_bool transport_faulted_{false};
  // Watchdog grace start: no failsafe until kLinkLossTimeout after Connect(), so
  // a just-opened link is not flagged before the first frame can arrive.
  std::chrono::steady_clock::time_point connect_time_{};

  std::atomic_bool keep_running_{false};
  std::thread watchdog_;
};
}  // namespace xmotion

#endif  // SBUS_RECEIVER_HPP

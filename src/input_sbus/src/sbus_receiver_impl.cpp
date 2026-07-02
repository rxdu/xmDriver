/*
 * @file sbus_receiver_impl.cpp
 * @date 11/23/24
 * @brief
 *
 * @copyright Copyright (c) 2024 Ruixiang Du (rdu)
 */

#include "input_sbus/sbus_receiver.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include "rpi_sbus/SBUS.h"

#include "xmsigma/logging/xlogger.hpp"
#include "xmsigma/types/time.hpp"

namespace xmotion {
namespace {
// Blocking read timeout (deciseconds): read() returns at least this often so the
// loop can check keep_running_ (fixes the shutdown hang) and run the watchdog.
constexpr uint8_t kReadTimeoutDs = 1;  // 100 ms
// If no valid SBUS frame arrives within this window, force a failsafe. SBUS
// frames arrive every ~7-14 ms, so this is many missed frames = link lost.
constexpr auto kLinkLossTimeout = std::chrono::milliseconds(200);
}  // namespace

class SbusReceiver::Impl {
 public:
  Impl(const std::string& port) : port_(port) {}

  ~Impl() { Close(); }

  bool Open() {
    sbus_.onPacket(std::bind(&Impl::OnPacket, this, std::placeholders::_1));
    // Blocking WITH a timeout: read() unblocks periodically so Close() can join
    // (the old blocking-forever install hung shutdown).
    sbus_err_t err = sbus_.install(port_.c_str(), true, kReadTimeoutDs);
    if (err != SBUS_OK) {
      XLOG_ERROR("SBUS install error on {}: {}", port_, static_cast<int>(err));
      return false;
    }

    keep_running_ = true;
    last_packet_time_ = Now();  // seed so we don't failsafe before the first read
    sbus_thread_ = std::thread([this]() {
      while (keep_running_) {
        sbus_err_t e = sbus_.read();
        if (e != SBUS_OK) {
          // Transient desync / timeout / read error — do NOT break the loop (the
          // old code exited the thread silently while IsOpened() still said
          // healthy). Keep running so the watchdog drives failsafe and we
          // auto-recover when frames resume.
          XLOG_DEBUG("SBUS read returned {}", static_cast<int>(e));
        }
        // Link-loss watchdog: no valid frame for too long -> emit failsafe so
        // the consumer stops acting on the last stick/kill state.
        if (Now() - last_packet_time_ > kLinkLossTimeout) EmitFailsafe();
      }
    });
    return true;
  }

  void Close() {
    keep_running_ = false;
    if (sbus_thread_.joinable()) sbus_thread_.join();
    sbus_.uninstall();
  }

  // Honest now: the read thread no longer dies silently, so keep_running_
  // actually reflects whether the receiver is running.
  bool IsOpened() const { return keep_running_; }

  void SetOnRcMessageReceivedCallback(OnRcMessageReceivedCallback cb) {
    std::lock_guard<std::mutex> lock(cb_mtx_);
    callback_ = cb;
  }

 private:
  void OnPacket(const sbus_packet_t& packet) {
    last_packet_time_ = Now();  // touched only on sbus_thread_

    RcMessage msg;
    for (int i = 0; i < 16; ++i) msg.channels[i] = packet.channels[i];
    msg.channels[16] = packet.ch17;
    msg.channels[17] = packet.ch18;
    msg.frame_loss = packet.frameLost;
    msg.fault_protection = packet.failsafe;  // receiver-reported failsafe

    std::lock_guard<std::mutex> lock(cb_mtx_);
    if (callback_) callback_(msg);
  }

  void EmitFailsafe() {
    RcMessage msg;  // channels default to 0
    msg.frame_loss = true;
    msg.fault_protection = true;
    std::lock_guard<std::mutex> lock(cb_mtx_);
    if (callback_) callback_(msg);
  }

  std::string port_;
  SBUS sbus_;

  std::mutex cb_mtx_;  // guards callback_ (set from any thread, used on RX thread)
  OnRcMessageReceivedCallback callback_;

  Timestamp last_packet_time_{};

  std::atomic_bool keep_running_{false};
  std::thread sbus_thread_;
};
}  // namespace xmotion

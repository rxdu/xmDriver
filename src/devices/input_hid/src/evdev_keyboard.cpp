/*
 * evdev_keyboard.cpp
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "input_hid/evdev_keyboard.hpp"

#include <linux/input.h>

#include <cstddef>
#include <utility>

#include "input_hid/details/evdev_reader.hpp"
#include "xmbase/telemetry/telemetry.hpp"

namespace xmotion {

EvdevKeyboard::EvdevKeyboard(Config cfg) : cfg_(std::move(cfg)) {}

EvdevKeyboard::EvdevKeyboard(std::string device_path)
    : EvdevKeyboard(Config{std::move(device_path)}) {}

EvdevKeyboard::~EvdevKeyboard() { Disconnect(); }

hal::Status EvdevKeyboard::Connect() {
  if (connected_.load()) return hal::Status::kOk;

  reader_ = std::make_unique<input_hid_detail::EvdevReader>(
      cfg_.device_path,
      [this](const input_event& ev) { HandleEvent(ev); },
      input_hid_detail::EvdevReader::OnReadyFn{},
      [this]() {
        connected_.store(false);
        XM_WARN("EvdevKeyboard: device {} disconnected", cfg_.device_path);
      });

  const hal::Status s = reader_->Open();
  if (s != hal::Status::kOk) {
    reader_.reset();
    return s;
  }

  connected_.store(true);
  XM_INFO("EvdevKeyboard: connected {} ({})", cfg_.device_path,
            reader_->DeviceName());
  return hal::Status::kOk;
}

void EvdevKeyboard::Disconnect() {
  if (reader_) {
    reader_->Close();
    reader_.reset();
    XM_INFO("EvdevKeyboard: disconnected {}", cfg_.device_path);
  }
  connected_.store(false);
}

bool EvdevKeyboard::IsConnected() const { return connected_.load(); }

hal::DeviceHealth EvdevKeyboard::Health() const {
  // Edge-triggered device: liveness is connection, not event recency. A
  // hot-unplug drops connected_ via the reader's -ENODEV detection.
  if (!connected_.load())
    return {hal::DeviceHealth::State::kDisconnected, "not connected"};
  return {hal::DeviceHealth::State::kOk, ""};
}

hal::Result<hal::KeyboardState> EvdevKeyboard::Read() {
  if (!connected_.load())
    return hal::Result<hal::KeyboardState>::Err(hal::Status::kNotConnected);
  // Hold-last-state: no kTimeout on an idle-but-connected device.
  std::lock_guard<std::mutex> lock(state_mtx_);
  return hal::Result<hal::KeyboardState>::Ok(state_);
}

void EvdevKeyboard::SetKeyEventCallback(KeyEventCallback cb) {
  std::lock_guard<std::mutex> lock(cb_mtx_);
  key_cb_ = std::move(cb);
}

void EvdevKeyboard::HandleEvent(const input_event& ev) {
  if (ev.type != EV_KEY) return;

  // .find() (not operator[]): read-only shared lookup; skip unmapped keys
  // instead of default-inserting into the shared map from the reader thread.
  const auto it = KeyboardMapping::keycode_map.find(ev.code);
  if (it == KeyboardMapping::keycode_map.end()) return;

  const hal::KeyboardCode code = it->second;
  const int value = ev.value;  // 1 = press, 2 = autorepeat, 0 = release
  {
    std::lock_guard<std::mutex> lock(state_mtx_);
    state_.pressed.set(static_cast<std::size_t>(code), value != 0);
    state_.stamp = std::chrono::steady_clock::now();
  }

  // Autorepeat (value 2) means "still held" — the key state already reflects it,
  // so don't re-fire kPress; only real down/up transitions emit an event.
  if (value == 2) return;

  KeyEventCallback cb;
  {
    std::lock_guard<std::mutex> lock(cb_mtx_);
    cb = key_cb_;
  }
  if (cb) {
    cb(code, value != 0 ? hal::KeyboardEvent::kPress
                        : hal::KeyboardEvent::kRelease);
  }
}

}  // namespace xmotion

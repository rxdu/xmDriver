/*
 * evdev_joystick.cpp
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "input_hid/evdev_joystick.hpp"

#include <linux/input.h>

#include <libevdev-1.0/libevdev/libevdev.h>

#include <utility>

#include "input_hid/details/evdev_reader.hpp"
#include "input_hid/details/evdev_translate.hpp"
#include "xmsigma/logging/xlogger.hpp"

namespace xmotion {

EvdevJoystick::EvdevJoystick(Config cfg) : cfg_(std::move(cfg)) {
  device_name_ = cfg_.device_path;
}

EvdevJoystick::EvdevJoystick(std::string device_path)
    : EvdevJoystick(Config{std::move(device_path)}) {}

EvdevJoystick::~EvdevJoystick() { Disconnect(); }

hal::Status EvdevJoystick::Connect() {
  if (connected_.load()) return hal::Status::kOk;

  reader_ = std::make_unique<input_hid_detail::EvdevReader>(
      cfg_.device_path,
      [this](const input_event& ev) { HandleEvent(ev); },
      [this](libevdev* dev) { OnDeviceReady(dev); },
      [this]() {
        connected_.store(false);
        XLOG_WARN("EvdevJoystick: device {} disconnected", cfg_.device_path);
      });

  const hal::Status s = reader_->Open();
  if (s != hal::Status::kOk) {
    reader_.reset();
    return s;
  }
  connected_.store(true);
  XLOG_INFO("EvdevJoystick: connected {} ({})", cfg_.device_path, device_name_);
  return hal::Status::kOk;
}

void EvdevJoystick::Disconnect() {
  if (reader_) {
    reader_->Close();
    reader_.reset();
    XLOG_INFO("EvdevJoystick: disconnected {}", cfg_.device_path);
  }
  connected_.store(false);
}

bool EvdevJoystick::IsConnected() const { return connected_.load(); }

hal::DeviceHealth EvdevJoystick::Health() const {
  // Edge-triggered device: liveness is connection, not event recency (an idle
  // joystick is still live). A hot-unplug drops connected_ via the reader.
  if (!connected_.load())
    return {hal::DeviceHealth::State::kDisconnected, "not connected"};
  return {hal::DeviceHealth::State::kOk, ""};
}

std::string EvdevJoystick::DeviceName() const {
  std::lock_guard<std::mutex> lock(state_mtx_);
  return device_name_;
}

hal::Result<hal::JoystickState> EvdevJoystick::Read() {
  if (!connected_.load())
    return hal::Result<hal::JoystickState>::Err(hal::Status::kNotConnected);
  // Hold-last-state: no kTimeout on an idle-but-connected device.
  std::lock_guard<std::mutex> lock(state_mtx_);
  return hal::Result<hal::JoystickState>::Ok(state_);
}

void EvdevJoystick::SetButtonCallback(ButtonCallback cb) {
  std::lock_guard<std::mutex> lock(cb_mtx_);
  button_cb_ = std::move(cb);
}

void EvdevJoystick::SetAxisCallback(AxisCallback cb) {
  std::lock_guard<std::mutex> lock(cb_mtx_);
  axis_cb_ = std::move(cb);
}

void EvdevJoystick::OnDeviceReady(libevdev* dev) {
  const char* name = libevdev_get_name(dev);
  {
    std::lock_guard<std::mutex> lock(state_mtx_);
    if (name != nullptr) device_name_ = name;
  }
  // Cache each modelled axis's reported range for [-1, 1] normalisation.
  for (std::size_t i = 0; i < kNumAxes; ++i) {
    const int code = input_hid_detail::JsAxisToAbsCode(static_cast<int>(i));
    if (code < 0) continue;
    if (const input_absinfo* info = libevdev_get_abs_info(dev, code)) {
      axis_min_[i] = info->minimum;
      axis_max_[i] = info->maximum;
    }
  }
}

void EvdevJoystick::HandleEvent(const input_event& ev) {
  if (ev.type == EV_KEY) {
    const int idx = input_hid_detail::JsButtonIndex(ev.code);
    if (idx < 0) return;  // not a joystick button we model (bounds-safe)
    const bool pressed = ev.value != 0;
    {
      std::lock_guard<std::mutex> lock(state_mtx_);
      state_.buttons[static_cast<std::size_t>(idx)] = pressed;
      state_.stamp = std::chrono::steady_clock::now();
    }

    ButtonCallback cb;
    {
      std::lock_guard<std::mutex> lock(cb_mtx_);
      cb = button_cb_;
    }
    if (cb) {
      cb(static_cast<hal::JsButton>(idx),
         pressed ? hal::ButtonEvent::kPress : hal::ButtonEvent::kRelease);
    }
  } else if (ev.type == EV_ABS) {
    const int idx = input_hid_detail::JsAxisIndex(ev.code);
    if (idx < 0) return;
    const std::size_t axis = static_cast<std::size_t>(idx);
    const float norm =
        input_hid_detail::NormalizeAxis(ev.value, axis_min_[axis], axis_max_[axis]);
    {
      std::lock_guard<std::mutex> lock(state_mtx_);
      state_.axes[axis] = norm;
      state_.stamp = std::chrono::steady_clock::now();
    }

    AxisCallback cb;
    {
      std::lock_guard<std::mutex> lock(cb_mtx_);
      cb = axis_cb_;
    }
    if (cb) cb(static_cast<hal::JsAxis>(idx), norm);
  }
}

}  // namespace xmotion

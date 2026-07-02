/*
 * joystick.cpp
 *
 * Created on 5/30/23 11:00 PM
 * Description:
 *
 * Copyright (c) 2023 Ruixiang Du (rdu)
 */

#include "input_hid/joystick.hpp"

#include <sys/ioctl.h>
#include <sys/inotify.h>

#include <fcntl.h>
#include <unistd.h>

#include <linux/input.h>
#include <linux/joystick.h>

#include <iostream>

#define PRINT_DEBUG_MSG 0

namespace xmotion {
static const char* js_button_names[32] = {
    "TRIGGER", "THUMB", "THUMB2", "TOP",   "TOP2", "PINKIE", "BASE",  "BASE2",
    "BASE3",   "BASE4", "BASE5",  "BASE6", "",     "",       "",      "DEAD",
    "SOUTH",   "EAST",  "C",      "NORTH", "WEST", "Z",      "TL",    "TR",
    "TL2",     "TR2",   "SELECT", "START", "MODE", "THUMBL", "THUMBR"};

static const char* js_axis_names[32] = {
    "X",        "Y",        "Z",      "RX",     "RY",        "RZ",
    "THROTTLE", "RUDDER",   "WHEEL",  "GAS",    "BRAKE",     "",
    "",         "",         "",       "",       "HAT0X",     "HAT0Y",
    "HAT1X",    "HAT1Y",    "HAT2X",  "HAT2Y",  "HAT3X",     "HAT3Y",
    "PRESSURE", "DISTANCE", "TILT_X", "TILT_Y", "TOOL_WIDTH"};

std::vector<JoystickDescriptor> Joystick::EnumberateJoysticks(int max_index) {
  std::vector<JoystickDescriptor> jss;

  char js_name[128];
  for (int i = 0; i < max_index; ++i) {
    std::string file_name = "/dev/input/event" + std::to_string(i);
    int fd = open(file_name.c_str(), O_RDWR | O_NONBLOCK);
    if (fd != -1) {
      JoystickDescriptor jd;
      ioctl(fd, EVIOCGNAME(sizeof(js_name)), js_name);
      jd.index = i;
      jd.name = {js_name};
      close(fd);
      jss.push_back(jd);
    }
  }
  return jss;
}

////////////////////////////////////////////////////////////////////////////////

Joystick::Joystick() {
  descriptor_.index = 0;
  auto jds = Joystick::EnumberateJoysticks();
  if (jds.empty()) throw std::runtime_error("No joystick found");
  descriptor_.index = jds.back().index;
  device_name_ = "/dev/input/event" + std::to_string(descriptor_.index);
  InitializeChannels();
}

Joystick::Joystick(JoystickDescriptor descriptor) : descriptor_(descriptor) {
  device_name_ = "/dev/input/event" + std::to_string(descriptor.index);
  InitializeChannels();
}

Joystick::Joystick(int index)
    : Joystick(JoystickDescriptor{index, "js" + std::to_string(index)}) {}

Joystick::Joystick(const std::string& event_name) {
  device_name_ = event_name;
  try {
    descriptor_.index = std::stoi(event_name.substr(16));
  } catch (std::invalid_argument& e) {
    descriptor_.index = -1;
  }
  descriptor_.name = event_name;
  InitializeChannels();
}

Joystick::~Joystick() {
  // Always tear down: the polling thread may be running even when connected_ is
  // false (e.g. after a hot-unplug via ReopenDevice); a still-joinable thread at
  // destruction would std::terminate. Close() is idempotent.
  Close();
}

void Joystick::InitializeChannels() {
  // initialize values
  for (int i = 0; i < max_js_buttons; ++i) buttons_[i] = false;
  for (int i = 0; i < max_js_axes; ++i) {
    axes_[i].min = 0;
    axes_[i].max = 0;
    axes_[i].value = 0;
  }
}

bool Joystick::Open() {
  if (connected_) return true;

  fd_ = open(device_name_.c_str(), O_RDWR | O_NONBLOCK);
  if (fd_ != -1) {
    connected_ = true;

    // Setup axes
    for (unsigned int i = 0; i < Joystick::max_js_axes; ++i) {
      input_absinfo axisInfo;
      if (ioctl(fd_, EVIOCGABS(i), &axisInfo) != -1) {
        axes_[i].min = axisInfo.minimum;
        axes_[i].max = axisInfo.maximum;
      }
    }

    // Setup rumble
    ff_effect effect = {0};
    effect.type = FF_RUMBLE;
    effect.id = -1;
    if (ioctl(fd_, EVIOCSFF, &effect) != -1) {
      rumble_effect_id_ = effect.id;
      has_rumble_ = true;
    }

    device_change_notify_ = inotify_init1(IN_NONBLOCK);
    inotify_add_watch(device_change_notify_, device_name_.c_str(), IN_ATTRIB);

    keep_running_ = true;
    io_thread_ = std::thread([this]() {
      while (keep_running_) {
        this->PollEvent();
        usleep(16000);
      }
    });
  }
  return connected_;
}

void Joystick::Close() {
  keep_running_ = false;
  if (io_thread_.joinable()) io_thread_.join();

  if (connected_) {
    close(fd_);
    fd_ = -1;
    connected_ = false;
  }
  if (device_change_notify_ >= 0) {  // was leaked on every Open/Close cycle
    close(device_change_notify_);
    device_change_notify_ = -1;
  }
}

void Joystick::ReopenDevice() {
  // Runs on the polling thread on hot-plug. Re-open ONLY the fd — never call
  // Close()/Open() here (they join/relaunch io_thread_, i.e. the current thread
  // joining itself -> std::system_error / terminate).
  if (fd_ != -1) {
    close(fd_);
    fd_ = -1;
  }
  fd_ = open(device_name_.c_str(), O_RDWR | O_NONBLOCK);
  connected_ = (fd_ != -1);
  if (connected_) {
    for (unsigned int i = 0; i < Joystick::max_js_axes; ++i) {
      input_absinfo axisInfo;
      if (ioctl(fd_, EVIOCGABS(i), &axisInfo) != -1) {
        axes_[i].min = axisInfo.minimum;
        axes_[i].max = axisInfo.maximum;
      }
    }
  }
}

bool Joystick::IsOpened() const { return connected_; }

std::string Joystick::GetButtonName(const JsButton& btn) const {
  return std::string(js_button_names[static_cast<int>(btn)]);
}

std::string Joystick::GetAxisName(const JsAxis& axis) const {
  return std::string(js_axis_names[static_cast<int>(axis)]);
}

bool Joystick::GetButtonState(const JsButton& btn) const {
  std::lock_guard<std::mutex> lock(buttons_mtx_);
  return buttons_[static_cast<int>(btn)];
}

JsAxisValue Joystick::GetAxisState(const JsAxis& axis) const {
  std::lock_guard<std::mutex> lock(axes_mtx_);
  return axes_[static_cast<int>(axis)];
}

void Joystick::ReadJoystickInput() {
  input_event event;
  while (read(fd_, &event, sizeof(event)) > 0) {
    {
      std::lock_guard<std::mutex> lock(buttons_mtx_);
      if (event.type == EV_KEY && event.code >= BTN_JOYSTICK &&
          event.code <= BTN_THUMBR) {
        buttons_[event.code - 0x120] = event.value;
      }
    }
    {
      std::lock_guard<std::mutex> lock(axes_mtx_);
      // Bound to the axes_ array (size max_js_axes = 32). The old guard used
      // ABS_TOOL_WIDTH (0x28 = 40), so codes 32..39 wrote out of bounds.
      if (event.type == EV_ABS && event.code < max_js_axes) {
        auto axis = &axes_[event.code];
        const int range = axis->max - axis->min;
        // Skip un-calibrated axes (min == max) — dividing by 0 yielded inf/NaN.
        if (range != 0) {
          axes_[event.code].value =
              (event.value - axis->min) / static_cast<float>(range) * 2 - 1;
        }
      }
    }
  }
}

void Joystick::PollEvent() {
  // PollEvent which joysticks are connected
  inotify_event event;
  if (read(device_change_notify_, &event, sizeof(event)) != -1) {
    // Hot-plug: re-open the fd in place. Must NOT Close()/Open() here — this
    // runs on io_thread_, and Close() joins io_thread_ (self-join -> terminate).
    ReopenDevice();
  }

  // PollEvent and print inputs for each connected joystick
  if (connected_) {
    ReadJoystickInput();

#if PRINT_DEBUG_MSG
    printf("%s - Axes: ", descriptor_.name.c_str());
    for (char axisIndex = 0; axisIndex < max_js_axes; ++axisIndex) {
      if (axes_[axisIndex].max - axes_[axisIndex].min)
        printf("%s:% f ", js_axis_names[axisIndex], axes_[axisIndex].value);
    }
    printf("Buttons: ");
    for (char buttonIndex = 0; buttonIndex < max_js_buttons; ++buttonIndex) {
      if (buttons_[buttonIndex]) printf("%s ", js_button_names[buttonIndex]);
    }
    printf("\n");
#endif
  }
#if PRINT_DEBUG_MSG
  fflush(stdout);
#endif
}

void Joystick::SetJoystickRumble(short weakRumble, short strongRumble) {
  if (has_rumble_) {
    ff_effect effect = {0};
    effect.type = FF_RUMBLE;
    effect.id = rumble_effect_id_;
    effect.replay.length = 5000;
    effect.replay.delay = 0;
    effect.u.rumble.weak_magnitude = weakRumble;
    effect.u.rumble.strong_magnitude = strongRumble;
    ioctl(fd_, EVIOCSFF, &effect);

    input_event play = {0};
    play.type = EV_FF;
    play.code = rumble_effect_id_;
    play.value = 1;
    ssize_t ret = write(fd_, &play, sizeof(play));
    (void)ret;
  }
}
}  // namespace xmotion
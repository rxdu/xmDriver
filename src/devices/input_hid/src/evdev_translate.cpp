/*
 * evdev_translate.cpp
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "input_hid/details/evdev_translate.hpp"

#include <linux/input-event-codes.h>

#include "xmdriver/hal/joystick.hpp"

namespace xmotion::input_hid_detail {

int JsButtonIndex(unsigned int code) {
  using B = hal::JsButton;
  switch (code) {
    case BTN_TRIGGER: return static_cast<int>(B::kTrigger);
    case BTN_THUMB:   return static_cast<int>(B::kThumb);
    case BTN_THUMB2:  return static_cast<int>(B::kThumb2);
    case BTN_TOP:     return static_cast<int>(B::kTop1);
    case BTN_TOP2:    return static_cast<int>(B::kTop2);
    case BTN_PINKIE:  return static_cast<int>(B::kPinkie);
    case BTN_BASE:    return static_cast<int>(B::kBase);
    case BTN_BASE2:   return static_cast<int>(B::kBase2);
    case BTN_BASE3:   return static_cast<int>(B::kBase3);
    case BTN_BASE4:   return static_cast<int>(B::kBase4);
    case BTN_BASE5:   return static_cast<int>(B::kBase5);
    case BTN_BASE6:   return static_cast<int>(B::kBase6);
    case BTN_SOUTH:   return static_cast<int>(B::kSouth);
    case BTN_EAST:    return static_cast<int>(B::kEast);
    case BTN_NORTH:   return static_cast<int>(B::kNorth);
    case BTN_WEST:    return static_cast<int>(B::kWest);
    case BTN_TL:      return static_cast<int>(B::kTL);
    case BTN_TR:      return static_cast<int>(B::kTR);
    case BTN_TL2:     return static_cast<int>(B::kTL2);
    case BTN_TR2:     return static_cast<int>(B::kTR2);
    case BTN_SELECT:  return static_cast<int>(B::kSelect);
    case BTN_START:   return static_cast<int>(B::kStart);
    case BTN_MODE:    return static_cast<int>(B::kMode);
    case BTN_THUMBL:  return static_cast<int>(B::kThumbL);
    case BTN_THUMBR:  return static_cast<int>(B::kThumbR);
    default:          return -1;
  }
}

int JsButtonToKeyCode(int button_index) {
  using B = hal::JsButton;
  switch (static_cast<B>(button_index)) {
    case B::kTrigger: return BTN_TRIGGER;
    case B::kThumb:   return BTN_THUMB;
    case B::kThumb2:  return BTN_THUMB2;
    case B::kTop1:    return BTN_TOP;
    case B::kTop2:    return BTN_TOP2;
    case B::kPinkie:  return BTN_PINKIE;
    case B::kBase:    return BTN_BASE;
    case B::kBase2:   return BTN_BASE2;
    case B::kBase3:   return BTN_BASE3;
    case B::kBase4:   return BTN_BASE4;
    case B::kBase5:   return BTN_BASE5;
    case B::kBase6:   return BTN_BASE6;
    case B::kSouth:   return BTN_SOUTH;
    case B::kEast:    return BTN_EAST;
    case B::kNorth:   return BTN_NORTH;
    case B::kWest:    return BTN_WEST;
    case B::kTL:      return BTN_TL;
    case B::kTR:      return BTN_TR;
    case B::kTL2:     return BTN_TL2;
    case B::kTR2:     return BTN_TR2;
    case B::kSelect:  return BTN_SELECT;
    case B::kStart:   return BTN_START;
    case B::kMode:    return BTN_MODE;
    case B::kThumbL:  return BTN_THUMBL;
    case B::kThumbR:  return BTN_THUMBR;
    default:          return -1;
  }
}

int JsAxisIndex(unsigned int code) {
  using A = hal::JsAxis;
  switch (code) {
    case ABS_X:        return static_cast<int>(A::kX);
    case ABS_Y:        return static_cast<int>(A::kY);
    case ABS_Z:        return static_cast<int>(A::kZ);
    case ABS_RX:       return static_cast<int>(A::kRX);
    case ABS_RY:       return static_cast<int>(A::kRY);
    case ABS_RZ:       return static_cast<int>(A::kRZ);
    case ABS_THROTTLE: return static_cast<int>(A::kThrottle);
    case ABS_RUDDER:   return static_cast<int>(A::kRudder);
    case ABS_WHEEL:    return static_cast<int>(A::kWheel);
    case ABS_GAS:      return static_cast<int>(A::kGas);
    case ABS_BRAKE:    return static_cast<int>(A::kBrake);
    case ABS_HAT0X:    return static_cast<int>(A::kHat0X);
    case ABS_HAT0Y:    return static_cast<int>(A::kHat0Y);
    case ABS_HAT1X:    return static_cast<int>(A::kHat1X);
    case ABS_HAT1Y:    return static_cast<int>(A::kHat1Y);
    default:           return -1;
  }
}

int JsAxisToAbsCode(int axis_index) {
  using A = hal::JsAxis;
  switch (static_cast<A>(axis_index)) {
    case A::kX:        return ABS_X;
    case A::kY:        return ABS_Y;
    case A::kZ:        return ABS_Z;
    case A::kRX:       return ABS_RX;
    case A::kRY:       return ABS_RY;
    case A::kRZ:       return ABS_RZ;
    case A::kThrottle: return ABS_THROTTLE;
    case A::kRudder:   return ABS_RUDDER;
    case A::kWheel:    return ABS_WHEEL;
    case A::kGas:      return ABS_GAS;
    case A::kBrake:    return ABS_BRAKE;
    case A::kHat0X:    return ABS_HAT0X;
    case A::kHat0Y:    return ABS_HAT0Y;
    case A::kHat1X:    return ABS_HAT1X;
    case A::kHat1Y:    return ABS_HAT1Y;
    default:           return -1;
  }
}

float NormalizeAxis(int value, int min, int max) {
  const int range = max - min;
  if (range == 0) return 0.0f;  // un-calibrated axis; avoid divide-by-zero
  return (value - min) / static_cast<float>(range) * 2.0f - 1.0f;
}

}  // namespace xmotion::input_hid_detail

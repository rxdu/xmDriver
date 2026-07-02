/*
 * evdev_translate.hpp
 *
 * Pure, device-independent translation between Linux evdev codes and the HAL
 * joystick model. Kept free of any device/fd state so it is unit-testable
 * without hardware, and keeps <linux/input-event-codes.h> out of the headers.
 *
 * The evdev button/axis codes are NOT a contiguous range that maps to the HAL
 * enums by a fixed offset (BTN_DEAD, BTN_C and the HAT axes leave gaps), so an
 * explicit mapping is used — this also makes the array index bounds-safe by
 * construction (the old `code - 0x120` indexing could run off the array).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#ifndef XMOTION_INPUT_HID_EVDEV_TRANSLATE_HPP
#define XMOTION_INPUT_HID_EVDEV_TRANSLATE_HPP

namespace xmotion::input_hid_detail {

// Maps an evdev EV_KEY code to a hal::JsButton index, or -1 if the code is not
// a joystick button we model.
int JsButtonIndex(unsigned int code);

// Maps an evdev EV_ABS code to a hal::JsAxis index, or -1 if not modelled.
int JsAxisIndex(unsigned int code);

// Inverse of JsAxisIndex: hal::JsAxis index -> evdev EV_ABS code, or -1.
int JsAxisToAbsCode(int axis_index);

// Normalises a raw abs value in [min, max] to [-1, 1]. Returns 0 when the range
// is degenerate (min == max, i.e. an un-calibrated axis) instead of dividing by
// zero (the old code produced inf/NaN).
float NormalizeAxis(int value, int min, int max);

}  // namespace xmotion::input_hid_detail

#endif  // XMOTION_INPUT_HID_EVDEV_TRANSLATE_HPP

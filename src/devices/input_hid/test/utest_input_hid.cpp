/*
 * utest_input_hid.cpp
 *
 * Hardware-independent unit tests for the input_hid drivers. Real device access
 * (libevdev over /dev/input/event*) is not available on CI, so this covers the
 * translation/normalisation logic and the key mapping — including the specific
 * defects the rewrite eliminates (axis divide-by-zero, out-of-bounds button
 * indexing).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <linux/input-event-codes.h>

#include <chrono>
#include <cmath>
#include <cstddef>
#include "gtest/gtest.h"

#include "input_hid/details/evdev_translate.hpp"
#include "input_hid/details/keyboard_mapping.hpp"
#include "xmmu/hal/joystick.hpp"
#include "xmmu/hal/keyboard.hpp"

using namespace xmotion;
using input_hid_detail::JsAxisIndex;
using input_hid_detail::JsAxisToAbsCode;
using input_hid_detail::JsButtonIndex;
using input_hid_detail::NormalizeAxis;

TEST(EvdevTranslate, ButtonCodesMapToHalEnum) {
  EXPECT_EQ(JsButtonIndex(BTN_TRIGGER), static_cast<int>(hal::JsButton::kTrigger));
  EXPECT_EQ(JsButtonIndex(BTN_SOUTH), static_cast<int>(hal::JsButton::kSouth));
  EXPECT_EQ(JsButtonIndex(BTN_NORTH), static_cast<int>(hal::JsButton::kNorth));
  EXPECT_EQ(JsButtonIndex(BTN_THUMBR), static_cast<int>(hal::JsButton::kThumbR));
}

TEST(EvdevTranslate, UnmappedButtonCodesAreRejected) {
  // BTN_DEAD / BTN_C sit in gaps of the HAL enum; the old `code - 0x120`
  // indexing would have written out of bounds. They must map to -1.
  EXPECT_EQ(JsButtonIndex(BTN_DEAD), -1);
  EXPECT_EQ(JsButtonIndex(BTN_C), -1);
  EXPECT_EQ(JsButtonIndex(KEY_A), -1);       // not a joystick button at all
  EXPECT_EQ(JsButtonIndex(0xffff), -1);      // wildly out of range
}

TEST(EvdevTranslate, EveryMappedButtonIndexIsInBounds) {
  for (unsigned int code = 0; code <= 0x2ff; ++code) {
    const int idx = JsButtonIndex(code);
    if (idx >= 0) {
      EXPECT_LT(idx, static_cast<int>(hal::JsButton::kLast)) << "code " << code;
    }
  }
}

TEST(EvdevTranslate, AxisCodesRoundTrip) {
  EXPECT_EQ(JsAxisIndex(ABS_X), static_cast<int>(hal::JsAxis::kX));
  EXPECT_EQ(JsAxisIndex(ABS_HAT0X), static_cast<int>(hal::JsAxis::kHat0X));
  EXPECT_EQ(JsAxisIndex(ABS_HAT1Y), static_cast<int>(hal::JsAxis::kHat1Y));
  // Inverse mapping is consistent for every modelled axis.
  for (int i = 0; i < static_cast<int>(hal::JsAxis::kLast); ++i) {
    const int code = JsAxisToAbsCode(i);
    ASSERT_GE(code, 0) << "axis " << i;
    EXPECT_EQ(JsAxisIndex(static_cast<unsigned int>(code)), i);
  }
}

TEST(EvdevTranslate, AxisNormalizationSpansMinusOneToOne) {
  EXPECT_FLOAT_EQ(NormalizeAxis(-32768, -32768, 32767), -1.0f);
  EXPECT_NEAR(NormalizeAxis(0, -32768, 32767), 0.0f, 1e-4);
  EXPECT_FLOAT_EQ(NormalizeAxis(32767, -32768, 32767), 1.0f);
}

TEST(EvdevTranslate, UncalibratedAxisDoesNotDivideByZero) {
  // min == max previously produced inf/NaN; must be a clean 0.
  const float v = NormalizeAxis(5, 0, 0);
  EXPECT_TRUE(std::isfinite(v));
  EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(KeyboardMappingTest, EvdevCodesMapToCanonicalCodes) {
  const auto& m = KeyboardMapping::keycode_map;
  ASSERT_NE(m.find(KEY_A), m.end());
  EXPECT_EQ(m.at(KEY_A), hal::KeyboardCode::kA);
  EXPECT_EQ(m.at(KEY_SPACE), hal::KeyboardCode::kSpace);
  EXPECT_EQ(m.at(KEY_LEFTCTRL), hal::KeyboardCode::kLeftCtrl);
}

TEST(KeyboardMappingTest, NameRoundTrip) {
  EXPECT_EQ(KeyboardMapping::GetKeyName(hal::KeyboardCode::kA), "A");
  EXPECT_EQ(KeyboardMapping::GetKeyCode("A"), hal::KeyboardCode::kA);
  EXPECT_EQ(KeyboardMapping::GetKeyCode("nonexistent"),
            hal::KeyboardCode::kUnknown);
}

TEST(KeyboardMappingTest, EveryMappedCodeIsWithinBitset) {
  for (const auto& [evdev_code, hal_code] : KeyboardMapping::keycode_map) {
    EXPECT_LT(static_cast<std::size_t>(hal_code),
              static_cast<std::size_t>(hal::KeyboardCode::kLast));
  }
}

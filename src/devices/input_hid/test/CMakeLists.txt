# The old feature/ demos (test_joystick*, test_keyboard*, test_multiple_device*)
# drove the removed polling/handler drivers and legacy interfaces; they were
# deleted in the HAL migration (ADR 0003). Device access needs a real HID device
# and cannot run on CI, so the unit test below covers the hardware-independent
# logic: evdev-code translation, axis normalisation (incl. the old
# divide-by-zero / OOB defects), the key mapping and the freshness contract.
add_executable(utest_input_hid utest_input_hid.cpp)
target_link_libraries(utest_input_hid input_hid gtest gmock gtest_main)
gtest_discover_tests(utest_input_hid)
add_test(NAME driver::input_hid COMMAND utest_input_hid)

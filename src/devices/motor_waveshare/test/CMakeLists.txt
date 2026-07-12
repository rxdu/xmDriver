add_executable(test_motor_waveshare test_motor_waveshare.cpp)
target_link_libraries(test_motor_waveshare motor_waveshare gtest gmock gtest_main)
gtest_discover_tests(test_motor_waveshare)
add_test(NAME driver::motor_waveshare COMMAND test_motor_waveshare)

add_executable(test_ddsm210_array test_ddsm210_array.cpp)
target_link_libraries(test_ddsm210_array motor_waveshare gtest gmock gtest_main)
gtest_discover_tests(test_ddsm210_array)
add_test(NAME driver::ddsm210_array COMMAND test_ddsm210_array)

# The SMS/STS array test drives the vendored synchronous protocol over a pty
# (util/openpty), same hardware-free seam as the async_port loopback tests.
add_executable(test_sms_sts_array test_sms_sts_array.cpp)
target_link_libraries(test_sms_sts_array motor_waveshare util gtest gmock gtest_main)
gtest_discover_tests(test_sms_sts_array)
add_test(NAME driver::sms_sts_array COMMAND test_sms_sts_array)

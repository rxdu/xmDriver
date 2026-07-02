add_executable(test_motor_akelc test_motor_akelc.cpp)
target_link_libraries(test_motor_akelc motor_akelc gtest gmock gtest_main)
gtest_discover_tests(test_motor_akelc)
add_test(NAME driver::motor_akelc COMMAND test_motor_akelc)

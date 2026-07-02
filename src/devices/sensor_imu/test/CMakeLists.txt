# Hardware-free unit test for the HiPNUC IMU driver on the new HAL.
add_executable(test_imu_hipnuc test_imu_hipnuc.cpp)
target_link_libraries(test_imu_hipnuc sensor_imu gtest gmock gtest_main)
gtest_discover_tests(test_imu_hipnuc)
add_test(NAME driver::imu_hipnuc COMMAND test_imu_hipnuc)

add_subdirectory(devel)

# Hardware-free unit tests for the migrated SbusReceiver (fake transport) and the
# SBUS decoder / scaling helpers.
add_executable(test_sbus_receiver test_sbus_receiver.cpp)
target_link_libraries(test_sbus_receiver PRIVATE input_sbus gtest gtest_main)
gtest_discover_tests(test_sbus_receiver)
add_test(NAME driver::sbus_receiver COMMAND test_sbus_receiver)

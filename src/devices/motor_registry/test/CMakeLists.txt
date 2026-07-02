add_executable(test_register_all test_register_all.cpp)
target_link_libraries(test_register_all
    PRIVATE gtest gmock gtest_main motor_registry)
gtest_discover_tests(test_register_all)
add_test(NAME driver::register_all COMMAND test_register_all)

# Hardware-free lifecycle/RAII test for the ModbusRtuPort libmodbus wrapper.
add_executable(utest_modbus_rtu utest_modbus_rtu.cpp)
target_link_libraries(utest_modbus_rtu modbus_rtu gtest gmock gtest_main)
gtest_discover_tests(utest_modbus_rtu)
add_test(NAME driver::modbus_rtu COMMAND utest_modbus_rtu)

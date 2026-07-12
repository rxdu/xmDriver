add_test([=[ModbusRtuPortLifecycle.ConstructDestructNoOpen]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_modbus_rtu [==[--gtest_filter=ModbusRtuPortLifecycle.ConstructDestructNoOpen]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ModbusRtuPortLifecycle.ConstructDestructNoOpen]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/modbus_rtu/test/utest_modbus_rtu.cpp:39]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/modbus_rtu/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ModbusRtuPortLifecycle.NotOpenedAfterConstruction]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_modbus_rtu [==[--gtest_filter=ModbusRtuPortLifecycle.NotOpenedAfterConstruction]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ModbusRtuPortLifecycle.NotOpenedAfterConstruction]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/modbus_rtu/test/utest_modbus_rtu.cpp:45]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/modbus_rtu/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ModbusRtuPortLifecycle.OpenBogusDeviceFailsClean]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_modbus_rtu [==[--gtest_filter=ModbusRtuPortLifecycle.OpenBogusDeviceFailsClean]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ModbusRtuPortLifecycle.OpenBogusDeviceFailsClean]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/modbus_rtu/test/utest_modbus_rtu.cpp:52]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/modbus_rtu/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ModbusRtuPortLifecycle.CloseWithoutOpenIsSafe]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_modbus_rtu [==[--gtest_filter=ModbusRtuPortLifecycle.CloseWithoutOpenIsSafe]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ModbusRtuPortLifecycle.CloseWithoutOpenIsSafe]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/modbus_rtu/test/utest_modbus_rtu.cpp:59]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/modbus_rtu/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ModbusRtuPortLifecycle.DoubleCloseIsSafe]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_modbus_rtu [==[--gtest_filter=ModbusRtuPortLifecycle.DoubleCloseIsSafe]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ModbusRtuPortLifecycle.DoubleCloseIsSafe]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/modbus_rtu/test/utest_modbus_rtu.cpp:67]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/modbus_rtu/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ModbusRtuPortLifecycle.ReopenAfterCloseNoLeak]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_modbus_rtu [==[--gtest_filter=ModbusRtuPortLifecycle.ReopenAfterCloseNoLeak]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ModbusRtuPortLifecycle.ReopenAfterCloseNoLeak]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/modbus_rtu/test/utest_modbus_rtu.cpp:77]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/modbus_rtu/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ModbusRtuPortLifecycle.SetResponseTimeoutWhileClosedFails]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_modbus_rtu [==[--gtest_filter=ModbusRtuPortLifecycle.SetResponseTimeoutWhileClosedFails]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ModbusRtuPortLifecycle.SetResponseTimeoutWhileClosedFails]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/modbus_rtu/test/utest_modbus_rtu.cpp:89]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/modbus_rtu/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ModbusRtuPortReadWrite.ReadWriteWhileNotConnectedReturnsError]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_modbus_rtu [==[--gtest_filter=ModbusRtuPortReadWrite.ReadWriteWhileNotConnectedReturnsError]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ModbusRtuPortReadWrite.ReadWriteWhileNotConnectedReturnsError]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/modbus_rtu/test/utest_modbus_rtu.cpp:96]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/modbus_rtu/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ModbusRtuPortReadWrite.ReadWriteAfterFailedOpenReturnsError]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_modbus_rtu [==[--gtest_filter=ModbusRtuPortReadWrite.ReadWriteAfterFailedOpenReturnsError]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ModbusRtuPortReadWrite.ReadWriteAfterFailedOpenReturnsError]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/modbus_rtu/test/utest_modbus_rtu.cpp:117]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/modbus_rtu/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(utest_modbus_rtu_TESTS [==[ModbusRtuPortLifecycle.ConstructDestructNoOpen]==] [==[ModbusRtuPortLifecycle.NotOpenedAfterConstruction]==] [==[ModbusRtuPortLifecycle.OpenBogusDeviceFailsClean]==] [==[ModbusRtuPortLifecycle.CloseWithoutOpenIsSafe]==] [==[ModbusRtuPortLifecycle.DoubleCloseIsSafe]==] [==[ModbusRtuPortLifecycle.ReopenAfterCloseNoLeak]==] [==[ModbusRtuPortLifecycle.SetResponseTimeoutWhileClosedFails]==] [==[ModbusRtuPortReadWrite.ReadWriteWhileNotConnectedReturnsError]==] [==[ModbusRtuPortReadWrite.ReadWriteAfterFailedOpenReturnsError]==])

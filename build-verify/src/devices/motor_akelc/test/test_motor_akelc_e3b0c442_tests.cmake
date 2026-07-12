add_test([=[MotorAkelcTest.RegistersWithFactory]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_motor_akelc [==[--gtest_filter=MotorAkelcTest.RegistersWithFactory]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MotorAkelcTest.RegistersWithFactory]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_akelc/test/test_motor_akelc.cpp:112]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_akelc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[MotorAkelcTest.ExposesSpeedAndBrakeButNotCurrentOrPosition]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_motor_akelc [==[--gtest_filter=MotorAkelcTest.ExposesSpeedAndBrakeButNotCurrentOrPosition]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MotorAkelcTest.ExposesSpeedAndBrakeButNotCurrentOrPosition]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_akelc/test/test_motor_akelc.cpp:126]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_akelc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[MotorAkelcTest.CommandsAndReadsRejectedWhenDisconnected]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_motor_akelc [==[--gtest_filter=MotorAkelcTest.CommandsAndReadsRejectedWhenDisconnected]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MotorAkelcTest.CommandsAndReadsRejectedWhenDisconnected]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_akelc/test/test_motor_akelc.cpp:135]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_akelc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[MotorAkelcTest.ConnectFailsWhenDeviceUnreachable]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_motor_akelc [==[--gtest_filter=MotorAkelcTest.ConnectFailsWhenDeviceUnreachable]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MotorAkelcTest.ConnectFailsWhenDeviceUnreachable]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_akelc/test/test_motor_akelc.cpp:147]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_akelc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[MotorAkelcTest.SetSpeedWithinRangeWritesTargetRegister]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_motor_akelc [==[--gtest_filter=MotorAkelcTest.SetSpeedWithinRangeWritesTargetRegister]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MotorAkelcTest.SetSpeedWithinRangeWritesTargetRegister]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_akelc/test/test_motor_akelc.cpp:158]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_akelc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[MotorAkelcTest.SetSpeedRejectsNonFinite]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_motor_akelc [==[--gtest_filter=MotorAkelcTest.SetSpeedRejectsNonFinite]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MotorAkelcTest.SetSpeedRejectsNonFinite]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_akelc/test/test_motor_akelc.cpp:168]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_akelc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[MotorAkelcTest.SetSpeedClampsAndReportsOutOfRange]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_motor_akelc [==[--gtest_filter=MotorAkelcTest.SetSpeedClampsAndReportsOutOfRange]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MotorAkelcTest.SetSpeedClampsAndReportsOutOfRange]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_akelc/test/test_motor_akelc.cpp:177]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_akelc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[MotorAkelcTest.StopZerosTargetRpm]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_motor_akelc [==[--gtest_filter=MotorAkelcTest.StopZerosTargetRpm]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MotorAkelcTest.StopZerosTargetRpm]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_akelc/test/test_motor_akelc.cpp:188]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_akelc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[MotorAkelcTest.DisconnectCommandsStopBeforeClosing]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_motor_akelc [==[--gtest_filter=MotorAkelcTest.DisconnectCommandsStopBeforeClosing]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MotorAkelcTest.DisconnectCommandsStopBeforeClosing]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_akelc/test/test_motor_akelc.cpp:197]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_akelc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[MotorAkelcTest.GetSpeedReturnsValueThenIoErrorOnBusFault]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_motor_akelc [==[--gtest_filter=MotorAkelcTest.GetSpeedReturnsValueThenIoErrorOnBusFault]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MotorAkelcTest.GetSpeedReturnsValueThenIoErrorOnBusFault]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_akelc/test/test_motor_akelc.cpp:208]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_akelc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[MotorAkelcTest.ApplyBrakeValidatesRatio]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_motor_akelc [==[--gtest_filter=MotorAkelcTest.ApplyBrakeValidatesRatio]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MotorAkelcTest.ApplyBrakeValidatesRatio]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_akelc/test/test_motor_akelc.cpp:224]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_akelc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[MotorAkelcTest.HealthFaultsOnDeviceErrorRegister]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_motor_akelc [==[--gtest_filter=MotorAkelcTest.HealthFaultsOnDeviceErrorRegister]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MotorAkelcTest.HealthFaultsOnDeviceErrorRegister]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_akelc/test/test_motor_akelc.cpp:241]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_akelc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[MotorAkelcModbusTest.FreshnessGoesStaleAfterTimeout]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_motor_akelc [==[--gtest_filter=MotorAkelcModbusTest.FreshnessGoesStaleAfterTimeout]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[MotorAkelcModbusTest.FreshnessGoesStaleAfterTimeout]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_akelc/test/test_motor_akelc.cpp:255]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_akelc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(test_motor_akelc_TESTS [==[MotorAkelcTest.RegistersWithFactory]==] [==[MotorAkelcTest.ExposesSpeedAndBrakeButNotCurrentOrPosition]==] [==[MotorAkelcTest.CommandsAndReadsRejectedWhenDisconnected]==] [==[MotorAkelcTest.ConnectFailsWhenDeviceUnreachable]==] [==[MotorAkelcTest.SetSpeedWithinRangeWritesTargetRegister]==] [==[MotorAkelcTest.SetSpeedRejectsNonFinite]==] [==[MotorAkelcTest.SetSpeedClampsAndReportsOutOfRange]==] [==[MotorAkelcTest.StopZerosTargetRpm]==] [==[MotorAkelcTest.DisconnectCommandsStopBeforeClosing]==] [==[MotorAkelcTest.GetSpeedReturnsValueThenIoErrorOnBusFault]==] [==[MotorAkelcTest.ApplyBrakeValidatesRatio]==] [==[MotorAkelcTest.HealthFaultsOnDeviceErrorRegister]==] [==[MotorAkelcModbusTest.FreshnessGoesStaleAfterTimeout]==])

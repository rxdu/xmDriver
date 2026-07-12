add_test([=[VescMotorTest.RegistersWithFactory]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorTest.RegistersWithFactory]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorTest.RegistersWithFactory]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:114]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorTest.ExposesSpeedCurrentBrakeButNotPosition]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorTest.ExposesSpeedCurrentBrakeButNotPosition]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorTest.ExposesSpeedCurrentBrakeButNotPosition]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:128]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorTest.CommandsAndReadsRejectedWhenDisconnected]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorTest.CommandsAndReadsRejectedWhenDisconnected]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorTest.CommandsAndReadsRejectedWhenDisconnected]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:137]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorTest.ConnectToNonexistentBusFailsCleanly]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorTest.ConnectToNonexistentBusFailsCleanly]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorTest.ConnectToNonexistentBusFailsCleanly]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:152]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorFakeTest.ConnectFailsWhenTransportWontOpen]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorFakeTest.ConnectFailsWhenTransportWontOpen]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorFakeTest.ConnectFailsWhenTransportWontOpen]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:162]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorFakeTest.SetSpeedRejectsNonFinite]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorFakeTest.SetSpeedRejectsNonFinite]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorFakeTest.SetSpeedRejectsNonFinite]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:170]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorFakeTest.SetCurrentRejectsNonFinite]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorFakeTest.SetCurrentRejectsNonFinite]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorFakeTest.SetCurrentRejectsNonFinite]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:181]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorFakeTest.SetSpeedInRangeSendsExactCommand]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorFakeTest.SetSpeedInRangeSendsExactCommand]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorFakeTest.SetSpeedInRangeSendsExactCommand]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:192]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorFakeTest.SetSpeedClampsAndReportsOutOfRange]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorFakeTest.SetSpeedClampsAndReportsOutOfRange]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorFakeTest.SetSpeedClampsAndReportsOutOfRange]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:200]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorFakeTest.SetCurrentClampsAndReportsOutOfRange]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorFakeTest.SetCurrentClampsAndReportsOutOfRange]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorFakeTest.SetCurrentClampsAndReportsOutOfRange]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:212]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorFakeTest.ReportsTransportFailureOnSend]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorFakeTest.ReportsTransportFailureOnSend]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorFakeTest.ReportsTransportFailureOnSend]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:228]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorFakeTest.StaleReadUntilStatusFrameArrives]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorFakeTest.StaleReadUntilStatusFrameArrives]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorFakeTest.StaleReadUntilStatusFrameArrives]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:235]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorFakeTest.StatusFrameForOtherIdIsIgnored]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorFakeTest.StatusFrameForOtherIdIsIgnored]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorFakeTest.StatusFrameForOtherIdIsIgnored]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:252]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorFakeTest.StopSendsZeroCurrent]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorFakeTest.StopSendsZeroCurrent]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorFakeTest.StopSendsZeroCurrent]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:260]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorFakeTest.DisconnectCommandsSafeStateThenCloses]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorFakeTest.DisconnectCommandsSafeStateThenCloses]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorFakeTest.DisconnectCommandsSafeStateThenCloses]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:268]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[VescMotorFakeTest.BusFaultSurfacesInHealth]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_vesc_motor [==[--gtest_filter=VescMotorFakeTest.BusFaultSurfacesInHealth]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[VescMotorFakeTest.BusFaultSurfacesInHealth]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_vesc/test/test_vesc_motor.cpp:281]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_vesc/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(test_vesc_motor_TESTS [==[VescMotorTest.RegistersWithFactory]==] [==[VescMotorTest.ExposesSpeedCurrentBrakeButNotPosition]==] [==[VescMotorTest.CommandsAndReadsRejectedWhenDisconnected]==] [==[VescMotorTest.ConnectToNonexistentBusFailsCleanly]==] [==[VescMotorFakeTest.ConnectFailsWhenTransportWontOpen]==] [==[VescMotorFakeTest.SetSpeedRejectsNonFinite]==] [==[VescMotorFakeTest.SetCurrentRejectsNonFinite]==] [==[VescMotorFakeTest.SetSpeedInRangeSendsExactCommand]==] [==[VescMotorFakeTest.SetSpeedClampsAndReportsOutOfRange]==] [==[VescMotorFakeTest.SetCurrentClampsAndReportsOutOfRange]==] [==[VescMotorFakeTest.ReportsTransportFailureOnSend]==] [==[VescMotorFakeTest.StaleReadUntilStatusFrameArrives]==] [==[VescMotorFakeTest.StatusFrameForOtherIdIsIgnored]==] [==[VescMotorFakeTest.StopSendsZeroCurrent]==] [==[VescMotorFakeTest.DisconnectCommandsSafeStateThenCloses]==] [==[VescMotorFakeTest.BusFaultSurfacesInHealth]==])

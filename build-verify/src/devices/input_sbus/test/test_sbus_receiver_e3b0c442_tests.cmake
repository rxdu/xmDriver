add_test([=[SbusReceiverTest.DisconnectedReadReportsNotConnected]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_sbus_receiver [==[--gtest_filter=SbusReceiverTest.DisconnectedReadReportsNotConnected]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[SbusReceiverTest.DisconnectedReadReportsNotConnected]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/input_sbus/test/test_sbus_receiver.cpp:73]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/input_sbus/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[SbusReceiverTest.ValidFrameYieldsFreshReadAndCallback]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_sbus_receiver [==[--gtest_filter=SbusReceiverTest.ValidFrameYieldsFreshReadAndCallback]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[SbusReceiverTest.ValidFrameYieldsFreshReadAndCallback]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/input_sbus/test/test_sbus_receiver.cpp:83]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/input_sbus/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[SbusReceiverTest.LinkLossYieldsTimeoutAndFailsafeCallback]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_sbus_receiver [==[--gtest_filter=SbusReceiverTest.LinkLossYieldsTimeoutAndFailsafeCallback]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[SbusReceiverTest.LinkLossYieldsTimeoutAndFailsafeCallback]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/input_sbus/test/test_sbus_receiver.cpp:110]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/input_sbus/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[SbusReceiverTest.ReceiverFrameLossFlagDegradesHealth]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_sbus_receiver [==[--gtest_filter=SbusReceiverTest.ReceiverFrameLossFlagDegradesHealth]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[SbusReceiverTest.ReceiverFrameLossFlagDegradesHealth]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/input_sbus/test/test_sbus_receiver.cpp:135]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/input_sbus/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[SbusReceiverTest.ScaleChannelValueGuardsDivideByZero]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_sbus_receiver [==[--gtest_filter=SbusReceiverTest.ScaleChannelValueGuardsDivideByZero]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[SbusReceiverTest.ScaleChannelValueGuardsDivideByZero]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/input_sbus/test/test_sbus_receiver.cpp:149]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/input_sbus/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(test_sbus_receiver_TESTS [==[SbusReceiverTest.DisconnectedReadReportsNotConnected]==] [==[SbusReceiverTest.ValidFrameYieldsFreshReadAndCallback]==] [==[SbusReceiverTest.LinkLossYieldsTimeoutAndFailsafeCallback]==] [==[SbusReceiverTest.ReceiverFrameLossFlagDegradesHealth]==] [==[SbusReceiverTest.ScaleChannelValueGuardsDivideByZero]==])

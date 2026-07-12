add_test([=[ImuHipnucTest.ReadWhileDisconnectedIsNotConnected]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_imu_hipnuc [==[--gtest_filter=ImuHipnucTest.ReadWhileDisconnectedIsNotConnected]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ImuHipnucTest.ReadWhileDisconnectedIsNotConnected]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/sensor_imu/test/test_imu_hipnuc.cpp:45]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/sensor_imu/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ImuHipnucTest.ConnectToMissingDeviceFailsCleanly]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_imu_hipnuc [==[--gtest_filter=ImuHipnucTest.ConnectToMissingDeviceFailsCleanly]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ImuHipnucTest.ConnectToMissingDeviceFailsCleanly]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/sensor_imu/test/test_imu_hipnuc.cpp:56]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/sensor_imu/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ImuHipnucTest.ConnectedButNoFrameIsStaleTimeout]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_imu_hipnuc [==[--gtest_filter=ImuHipnucTest.ConnectedButNoFrameIsStaleTimeout]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ImuHipnucTest.ConnectedButNoFrameIsStaleTimeout]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/sensor_imu/test/test_imu_hipnuc.cpp:69]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/sensor_imu/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[ImuHipnucTest.SetSampleCallbackDoesNotFireWithoutFrames]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_imu_hipnuc [==[--gtest_filter=ImuHipnucTest.SetSampleCallbackDoesNotFireWithoutFrames]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[ImuHipnucTest.SetSampleCallbackDoesNotFireWithoutFrames]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/sensor_imu/test/test_imu_hipnuc.cpp:83]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/sensor_imu/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(test_imu_hipnuc_TESTS [==[ImuHipnucTest.ReadWhileDisconnectedIsNotConnected]==] [==[ImuHipnucTest.ConnectToMissingDeviceFailsCleanly]==] [==[ImuHipnucTest.ConnectedButNoFrameIsStaleTimeout]==] [==[ImuHipnucTest.SetSampleCallbackDoesNotFireWithoutFrames]==])

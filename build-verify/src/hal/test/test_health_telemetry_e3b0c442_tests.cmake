add_test([=[HealthMappingTest.MapsAllStatesOneToOne]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_health_telemetry [==[--gtest_filter=HealthMappingTest.MapsAllStatesOneToOne]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[HealthMappingTest.MapsAllStatesOneToOne]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/hal/test/test_health_telemetry.cpp:92]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/hal/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[HealthTelemetryTest.ReportsTransitionsOnlyWithMappedStates]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_health_telemetry [==[--gtest_filter=HealthTelemetryTest.ReportsTransitionsOnlyWithMappedStates]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[HealthTelemetryTest.ReportsTransitionsOnlyWithMappedStates]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/hal/test/test_health_telemetry.cpp:103]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/hal/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[HealthTelemetryTest.DeviceHealthOverloadUsesStateDetail]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_health_telemetry [==[--gtest_filter=HealthTelemetryTest.DeviceHealthOverloadUsesStateDetail]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[HealthTelemetryTest.DeviceHealthOverloadUsesStateDetail]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/hal/test/test_health_telemetry.cpp:128]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/hal/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(test_health_telemetry_TESTS [==[HealthMappingTest.MapsAllStatesOneToOne]==] [==[HealthTelemetryTest.ReportsTransitionsOnlyWithMappedStates]==] [==[HealthTelemetryTest.DeviceHealthOverloadUsesStateDetail]==])

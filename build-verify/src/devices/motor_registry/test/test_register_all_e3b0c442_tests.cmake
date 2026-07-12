add_test([=[RegisterAllMotors.RegistersEveryBundledType]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_register_all [==[--gtest_filter=RegisterAllMotors.RegistersEveryBundledType]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[RegisterAllMotors.RegistersEveryBundledType]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_registry/test/test_register_all.cpp:16]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_registry/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[RegisterAllMotors.SimMotorIsConstructibleFromConfig]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_register_all [==[--gtest_filter=RegisterAllMotors.SimMotorIsConstructibleFromConfig]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[RegisterAllMotors.SimMotorIsConstructibleFromConfig]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_registry/test/test_register_all.cpp:28]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_registry/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[RegisterAllMotors.UnknownTypeCreatesNull]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_register_all [==[--gtest_filter=RegisterAllMotors.UnknownTypeCreatesNull]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[RegisterAllMotors.UnknownTypeCreatesNull]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/devices/motor_registry/test/test_register_all.cpp:38]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/devices/motor_registry/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(test_register_all_TESTS [==[RegisterAllMotors.RegistersEveryBundledType]==] [==[RegisterAllMotors.SimMotorIsConstructibleFromConfig]==] [==[RegisterAllMotors.UnknownTypeCreatesNull]==])

add_test([=[AsyncSerialLifecycle.OpenNonexistentPortFailsCleanly]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_async_port [==[--gtest_filter=AsyncSerialLifecycle.OpenNonexistentPortFailsCleanly]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[AsyncSerialLifecycle.OpenNonexistentPortFailsCleanly]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/async_port/test/unit_test/utest_async_lifecycle.cpp:25]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/async_port/test/unit_test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[AsyncSerialLifecycle.SendBeforeOpenIsRejected]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_async_port [==[--gtest_filter=AsyncSerialLifecycle.SendBeforeOpenIsRejected]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[AsyncSerialLifecycle.SendBeforeOpenIsRejected]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/async_port/test/unit_test/utest_async_lifecycle.cpp:33]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/async_port/test/unit_test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[AsyncCanLifecycle.OpenNonexistentIfaceFailsCleanly]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_async_port [==[--gtest_filter=AsyncCanLifecycle.OpenNonexistentIfaceFailsCleanly]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[AsyncCanLifecycle.OpenNonexistentIfaceFailsCleanly]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/async_port/test/unit_test/utest_async_lifecycle.cpp:40]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/async_port/test/unit_test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[AsyncCanLifecycle.SendFrameBeforeOpenIsRejected]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_async_port [==[--gtest_filter=AsyncCanLifecycle.SendFrameBeforeOpenIsRejected]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[AsyncCanLifecycle.SendFrameBeforeOpenIsRejected]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/async_port/test/unit_test/utest_async_lifecycle.cpp:48]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/async_port/test/unit_test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(utest_async_port_TESTS [==[AsyncSerialLifecycle.OpenNonexistentPortFailsCleanly]==] [==[AsyncSerialLifecycle.SendBeforeOpenIsRejected]==] [==[AsyncCanLifecycle.OpenNonexistentIfaceFailsCleanly]==] [==[AsyncCanLifecycle.SendFrameBeforeOpenIsRejected]==])

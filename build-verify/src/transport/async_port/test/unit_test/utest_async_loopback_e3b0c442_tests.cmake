add_test([=[AsyncSerialLoopback.ReceiveDeliversBytesWrittenToMaster]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_async_loopback [==[--gtest_filter=AsyncSerialLoopback.ReceiveDeliversBytesWrittenToMaster]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[AsyncSerialLoopback.ReceiveDeliversBytesWrittenToMaster]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/async_port/test/unit_test/utest_async_loopback.cpp:95]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/async_port/test/unit_test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[AsyncSerialLoopback.SendBytesReachesMasterSide]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_async_loopback [==[--gtest_filter=AsyncSerialLoopback.SendBytesReachesMasterSide]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[AsyncSerialLoopback.SendBytesReachesMasterSide]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/async_port/test/unit_test/utest_async_loopback.cpp:127]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/async_port/test/unit_test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(utest_async_loopback_TESTS [==[AsyncSerialLoopback.ReceiveDeliversBytesWrittenToMaster]==] [==[AsyncSerialLoopback.SendBytesReachesMasterSide]==])

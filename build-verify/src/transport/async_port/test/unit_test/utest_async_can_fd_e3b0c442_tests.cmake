add_test([=[AsyncCanFd.ReceiveDeliversConvertedFrame]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_async_can_fd [==[--gtest_filter=AsyncCanFd.ReceiveDeliversConvertedFrame]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[AsyncCanFd.ReceiveDeliversConvertedFrame]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/async_port/test/unit_test/utest_async_can_fd.cpp:52]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/async_port/test/unit_test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[AsyncCanFd.PeerCloseFiresErrorCallback]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_async_can_fd [==[--gtest_filter=AsyncCanFd.PeerCloseFiresErrorCallback]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[AsyncCanFd.PeerCloseFiresErrorCallback]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/async_port/test/unit_test/utest_async_can_fd.cpp:93]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/async_port/test/unit_test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[AsyncCanFd.SendFrameReportsQueueFullUnderBackpressure]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/utest_async_can_fd [==[--gtest_filter=AsyncCanFd.SendFrameReportsQueueFullUnderBackpressure]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[AsyncCanFd.SendFrameReportsQueueFullUnderBackpressure]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/transport/async_port/test/unit_test/utest_async_can_fd.cpp:128]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/transport/async_port/test/unit_test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(utest_async_can_fd_TESTS [==[AsyncCanFd.ReceiveDeliversConvertedFrame]==] [==[AsyncCanFd.PeerCloseFiresErrorCallback]==] [==[AsyncCanFd.SendFrameReportsQueueFullUnderBackpressure]==])

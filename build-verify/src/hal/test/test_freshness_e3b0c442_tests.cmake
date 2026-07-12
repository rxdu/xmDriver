add_test([=[FreshnessMonitor.NeverUpdatedIsNotFresh]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_freshness [==[--gtest_filter=FreshnessMonitor.NeverUpdatedIsNotFresh]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FreshnessMonitor.NeverUpdatedIsNotFresh]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/hal/test/test_freshness.cpp:16]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/hal/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[FreshnessMonitor.MarkMakesFresh]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_freshness [==[--gtest_filter=FreshnessMonitor.MarkMakesFresh]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FreshnessMonitor.MarkMakesFresh]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/hal/test/test_freshness.cpp:22]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/hal/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[FreshnessMonitor.GoesStaleAfterTimeout]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_freshness [==[--gtest_filter=FreshnessMonitor.GoesStaleAfterTimeout]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FreshnessMonitor.GoesStaleAfterTimeout]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/hal/test/test_freshness.cpp:29]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/hal/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[FreshnessMonitor.ResetClearsUpdated]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_freshness [==[--gtest_filter=FreshnessMonitor.ResetClearsUpdated]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[FreshnessMonitor.ResetClearsUpdated]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/hal/test/test_freshness.cpp:38]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/hal/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
add_test([=[HealthFromFreshness.MapsStates]=]  /tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/bin/test_freshness [==[--gtest_filter=HealthFromFreshness.MapsStates]==] --gtest_also_run_disabled_tests)
set_tests_properties([=[HealthFromFreshness.MapsStates]=]
  PROPERTIES
    
    DEF_SOURCE_LINE [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/src/hal/test/test_freshness.cpp:46]==]
    WORKING_DIRECTORY [==[/tmp/claude-1000/-home-rdu-RduWs-robotics-toolbox-xmotion/9300b8f4-022a-4efc-9161-e3f8f9e8bea0/scratchpad/driver-bus/build-verify/src/hal/test]==]
    SKIP_REGULAR_EXPRESSION [==[\[  SKIPPED \]]==]
    
)
set(test_freshness_TESTS [==[FreshnessMonitor.NeverUpdatedIsNotFresh]==] [==[FreshnessMonitor.MarkMakesFresh]==] [==[FreshnessMonitor.GoesStaleAfterTimeout]==] [==[FreshnessMonitor.ResetClearsUpdated]==] [==[HealthFromFreshness.MapsStates]==])

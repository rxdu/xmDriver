# Hardware-free unit tests for the Mobile-Base protocol: E2E integrity, the core
# codec (round-trips + reject paths), the base-side command tracker, and the
# swerve reference profile.
foreach (t e2e core_codec command_tracker swerve_v1)
  add_executable(test_mobile_base_${t} test_${t}.cpp)
  target_link_libraries(test_mobile_base_${t} PRIVATE mobile_base gtest gtest_main)
  gtest_discover_tests(test_mobile_base_${t})
  add_test(NAME robots::mobile_base_${t} COMMAND test_mobile_base_${t})
endforeach ()

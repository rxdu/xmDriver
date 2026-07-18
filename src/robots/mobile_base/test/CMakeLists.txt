# Hardware-free unit tests for the Mobile-Base protocol: E2E integrity, the core
# codec (round-trips + reject paths), the base-side command tracker, and the
# swerve reference profile.
foreach (t e2e core_codec command_tracker swerve_v1)
  add_executable(test_mobile_base_${t} test_${t}.cpp)
  target_link_libraries(test_mobile_base_${t} PRIVATE mobile_base gtest gtest_main)
  gtest_discover_tests(test_mobile_base_${t})
  add_test(NAME robots::mobile_base_${t} COMMAND test_mobile_base_${t})
endforeach ()

# Commander-side driver test — links the STATIC mobile_base_driver (+ async_port).
add_executable(test_mobile_base_driver test_mobile_base_driver.cpp)
target_link_libraries(test_mobile_base_driver PRIVATE mobile_base_driver gtest gtest_main)
gtest_discover_tests(test_mobile_base_driver)
add_test(NAME robots::mobile_base_driver COMMAND test_mobile_base_driver)

# DBC sync test — needs the dictionary path baked in at compile time.
add_executable(test_mobile_base_dbc_sync test_dbc_sync.cpp)
target_link_libraries(test_mobile_base_dbc_sync PRIVATE mobile_base gtest gtest_main)
target_compile_definitions(test_mobile_base_dbc_sync PRIVATE
    MOBILE_BASE_DBC_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../docs/mobile_base.dbc")
gtest_discover_tests(test_mobile_base_dbc_sync)
add_test(NAME robots::mobile_base_dbc_sync COMMAND test_mobile_base_dbc_sync)

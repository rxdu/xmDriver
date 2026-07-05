# Unit tests for the header-only HAL interface layer (exercised via the
# reference SimMotorController). Links the bundled GoogleTest and the INTERFACE
# `hal` target (brings the include path + xmBase).
add_executable(test_hal_interfaces test_hal_interfaces.cpp)
target_link_libraries(test_hal_interfaces PRIVATE gtest gmock gtest_main hal)
gtest_discover_tests(test_hal_interfaces)
add_test(NAME gtest_hal_interfaces COMMAND test_hal_interfaces)

add_executable(test_freshness test_freshness.cpp)
target_link_libraries(test_freshness PRIVATE gtest gmock gtest_main hal)
gtest_discover_tests(test_freshness)
add_test(NAME gtest_freshness COMMAND test_freshness)

# DeviceHealth -> telemetry HealthState mapping + transition-only reporting,
# observed through a minimal capture binding (no SDK needed).
add_executable(test_health_telemetry test_health_telemetry.cpp)
target_link_libraries(test_health_telemetry PRIVATE gtest gmock gtest_main hal)
gtest_discover_tests(test_health_telemetry)
add_test(NAME gtest_health_telemetry COMMAND test_health_telemetry)

# Actuator groups (fan-out order, failure aggregation, Stop failsafe,
# fault-counter/warn instrumentation via the capture binding).
add_executable(test_actuator_group test_actuator_group.cpp)
target_link_libraries(test_actuator_group PRIVATE gtest gmock gtest_main hal)
gtest_discover_tests(test_actuator_group)
add_test(NAME gtest_actuator_group COMMAND test_actuator_group)

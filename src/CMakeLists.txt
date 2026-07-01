# Host hardware driver modules, organized by role tier (see docs/adr/0001).
# Build order follows inter-module dependencies: hal (interfaces) first, then the
# transport tier consumed by the motor / sensor / input drivers.
#
# Every module is an independently-linkable library (xmotion::<module>), so a
# consumer can pull in only what it needs (dependency minimization). The role
# tiers and the xmMu aggregate below are conveniences layered on top — not a
# coupling. Each module's own CMakeLists is a single xmmu_add_module() call
# (see cmake/xmmu_module.cmake), which also creates the xmotion::<module> alias.

# interfaces: device HAL and, as a separate peer layer below it, the transport
# interfaces (serial / CAN / Modbus contracts).
add_subdirectory(hal)
add_subdirectory(transport)

# transport tier: implementations of the transport interfaces
add_subdirectory(async_port)
add_subdirectory(modbus_rtu)

# motors tier
add_subdirectory(motor_akelc)
add_subdirectory(motor_vesc)
add_subdirectory(motor_waveshare)

# sensors tier
add_subdirectory(sensor_imu)

# input tier
add_subdirectory(input_hid)
add_subdirectory(input_sbus)

## Role-tier convenience targets — link a tier (xmotion::transport / ::motors /
## ::sensors / ::input) instead of enumerating its component libraries. The
## individual module libraries remain available for minimal-dependency consumers.
xmmu_add_group(transport transport_interface async_port modbus_rtu)
xmmu_add_group(motors    motor_akelc motor_vesc motor_waveshare)
xmmu_add_group(sensors   sensor_imu)
xmmu_add_group(input     input_hid input_sbus)

## Aggregate everything: consumers link a single xmotion::xmMu.
add_library(xmMu INTERFACE)
target_link_libraries(xmMu INTERFACE hal transport motors sensors input)
add_library(xmotion::xmMu ALIAS xmMu)
install(TARGETS xmMu
    EXPORT xmMuTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)

# Host hardware driver modules, organized by role tier (see docs/adr/0001).
# Build order follows inter-module dependencies: hal (interfaces) first, then the
# transport tier consumed by the motor / sensor / input drivers.
#
# Every module is an independently-linkable library (xmotion::<module>), so a
# consumer can pull in only what it needs (dependency minimization). The role
# tiers and the xmMu aggregate below are conveniences layered on top — not a
# coupling. Each module's own CMakeLists is a single xmmu_add_module() call
# (see cmake/xmmu_module.cmake), which also creates the xmotion::<module> alias.

# libmodbus is resolved once here (parent scope) so LIBMODBUS_FOUND and the
# PkgConfig::LIBMODBUS imported target are visible to every module that needs
# them (transport/modbus_rtu and motor_akelc), regardless of directory nesting.
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMODBUS QUIET IMPORTED_TARGET libmodbus)

# interfaces (device HAL) + the transport layer (interface + async_port/modbus
# implementations) grouped under src/transport/.
add_subdirectory(hal)
add_subdirectory(transport)

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

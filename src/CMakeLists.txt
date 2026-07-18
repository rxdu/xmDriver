# Host hardware driver modules, organized by role tier (see docs/adr/0001).
# Build order follows inter-module dependencies: hal (interfaces) first, then the
# transport tier consumed by the motor / sensor / input drivers.
#
# Every module is an independently-linkable library (xmotion::<module>), so a
# consumer can pull in only what it needs (dependency minimization). The role
# tiers and the xmDriver aggregate below are conveniences layered on top — not a
# coupling. Each module's own CMakeLists is a single xmdriver_add_module() call
# (see cmake/xmdriver_module.cmake), which also creates the xmotion::<module> alias.

# libmodbus is resolved once here (parent scope) so LIBMODBUS_FOUND and the
# PkgConfig::LIBMODBUS imported target are visible to every module that needs
# them (transport/modbus_rtu and motor_akelc), regardless of directory nesting.
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMODBUS QUIET IMPORTED_TARGET libmodbus)

# interfaces (device HAL) + the transport layer (interface + async_port/modbus
# implementations) grouped under src/transport/.
add_subdirectory(hal)
add_subdirectory(transport)

# device drivers (motors / sensors / input), grouped under src/devices/
add_subdirectory(devices)

# robot-layer drivers — a whole robot/base addressed as one entity over a bus
# (vs. the physical-component drivers under src/devices/), grouped under src/robots/
add_subdirectory(robots)

## Role-tier convenience targets — link a tier (xmotion::transport / ::motors /
## ::sensors / ::input / ::robots) instead of enumerating its component libraries.
## The individual module libraries remain available for minimal-dependency consumers.
xmdriver_add_group(transport transport_interface async_port modbus_rtu)
xmdriver_add_group(motors    motor_akelc motor_vesc motor_waveshare motor_registry)
xmdriver_add_group(sensors   sensor_imu)
xmdriver_add_group(input     input_hid input_sbus)
xmdriver_add_group(robots    mobile_base mobile_base_interface mobile_base_can_client)

## Aggregate everything: consumers link a single xmotion::xmDriver.
add_library(xmDriver INTERFACE)
target_link_libraries(xmDriver INTERFACE hal transport motors sensors input robots)
add_library(xmotion::xmDriver ALIAS xmDriver)
install(TARGETS xmDriver
    EXPORT xmDriverTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)

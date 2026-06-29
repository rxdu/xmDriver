# Host hardware driver modules. Build order follows inter-module dependencies:
# async_port and modbus_rtu provide transport layers consumed by the motor,
# sensor and input drivers.
#
# hal is the header-only hardware abstraction layer (driver interfaces) that the
# driver modules implement, so it must be added before them.
add_subdirectory(hal)

add_subdirectory(async_port)
add_subdirectory(modbus_rtu)

add_subdirectory(motor_akelc)
add_subdirectory(motor_vesc)
add_subdirectory(motor_waveshare)

add_subdirectory(sensor_imu)

add_subdirectory(input_hid)
add_subdirectory(input_sbus)

## Aggregate interface target: consumers link a single `xmotion::xmMu` to pull
## in every host hardware driver module.
add_library(xmMu INTERFACE)
target_link_libraries(xmMu INTERFACE
    hal
    async_port
    modbus_rtu
    motor_akelc
    motor_vesc
    motor_waveshare
    sensor_imu
    input_hid
    input_sbus)

## Namespaced ALIAS targets so add_subdirectory (in-tree) consumers see the same
## xmotion:: names as find_package consumers get via the export namespace below.
## Without these, a superbuild that bundles xmMu (xmNabla, the umbrella) cannot
## resolve xmotion::xmMu / xmotion::<driver> and would re-add its own copy.
add_library(xmotion::xmMu ALIAS xmMu)
foreach(_xm_drv async_port modbus_rtu motor_akelc motor_vesc motor_waveshare
                sensor_imu input_hid input_sbus)
  add_library(xmotion::${_xm_drv} ALIAS ${_xm_drv})
endforeach()

install(TARGETS xmMu
    EXPORT xmMuTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)

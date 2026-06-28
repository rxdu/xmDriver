# Host hardware driver modules. Build order follows inter-module dependencies:
# async_port and modbus_rtu provide transport layers consumed by the motor,
# sensor and input drivers.
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
    async_port
    modbus_rtu
    motor_akelc
    motor_vesc
    motor_waveshare
    sensor_imu
    input_hid
    input_sbus)

install(TARGETS xmMu
    EXPORT xmMuTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)

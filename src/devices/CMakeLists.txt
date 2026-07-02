# Device drivers — motors, sensors, and human-input peripherals. Each is an
# independent library (xmotion::<module>) that implements a device HAL interface
# and links the transport impl(s) it needs; grouped here so the tree shows the
# layer. The motors/sensors/input role tiers are convenience group targets
# defined in the parent CMakeLists.
add_subdirectory(motor_akelc)
add_subdirectory(motor_vesc)
add_subdirectory(motor_waveshare)

add_subdirectory(sensor_imu)

add_subdirectory(input_hid)
add_subdirectory(input_sbus)

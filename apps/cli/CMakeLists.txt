# xmdriver_cli — one binary, a subcommand per device, for hardware bring-up.
add_executable(xmdriver_cli
    xmdriver_cli.cpp
    cli_common.cpp
    cmd_sbus.cpp
    cmd_ddsm.cpp
    cmd_servo.cpp
    cmd_imu.cpp)

# Link every device module the CLI drives; their PUBLIC deps (async_port, hal,
# xmBase, Eigen) come transitively.
target_link_libraries(xmdriver_cli PRIVATE
    input_sbus
    motor_waveshare
    sensor_imu
    async_port
    xmotion::xmBase)

target_compile_options(xmdriver_cli PRIVATE -Wall -Wextra)
set_target_properties(xmdriver_cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

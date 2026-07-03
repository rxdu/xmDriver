find_package(Threads REQUIRED)

# ADR 0002: rpi_sbus dropped — framing is the first-party SbusDecoder over the
# async_port AsyncSerial transport (ASIO_ENABLE_OLD_SERVICES, as for sensor_imu).
# The bundled rpi_sbus submodule has been removed from third_party/ + .gitmodules.
xmdriver_add_module(input_sbus
    SOURCES src/sbus_decoder.cpp src/sbus_receiver.cpp
    PUBLIC_DEPS xmotion::hal async_port Threads::Threads
    PUBLIC_DEFINITIONS ASIO_ENABLE_OLD_SERVICES)

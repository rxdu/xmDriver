find_package(Threads REQUIRED)

xmdriver_add_module(sensor_imu
    SOURCES src/ch_serial.c src/imu_hipnuc.cpp
    PUBLIC_DEPS xmotion::hal async_port Threads::Threads
    PUBLIC_DEFINITIONS ASIO_ENABLE_OLD_SERVICES)

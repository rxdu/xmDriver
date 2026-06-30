find_package(Threads REQUIRED)

xmmu_add_module(async_port
    SOURCES src/async_serial.cpp src/async_can.cpp
    PUBLIC_DEPS xmotion::hal Threads::Threads
    PUBLIC_DEFINITIONS ASIO_ENABLE_OLD_SERVICES)

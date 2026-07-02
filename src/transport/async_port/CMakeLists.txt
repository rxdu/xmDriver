find_package(Threads REQUIRED)

xmmu_add_module(async_port
    SOURCES src/async_serial.cpp src/async_can.cpp
    # transport impl: realizes the transport interfaces (not the device HAL);
    # xmSigma for the RingBuffer used in the tx path.
    PUBLIC_DEPS xmotion::transport_interface xmotion::xmSigma Threads::Threads
    PUBLIC_DEFINITIONS ASIO_ENABLE_OLD_SERVICES)

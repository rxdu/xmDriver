find_package(Threads REQUIRED)

xmdriver_add_module(async_port
    SOURCES src/async_serial.cpp src/async_can.cpp
    # transport impl: realizes the transport interfaces (not the device HAL);
    # xmBase for the RingBuffer used in the tx path.
    PUBLIC_DEPS xmotion::transport_interface xmotion::xmBase Threads::Threads
    PUBLIC_DEFINITIONS ASIO_ENABLE_OLD_SERVICES)

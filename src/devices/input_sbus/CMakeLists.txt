find_package(Threads REQUIRED)

xmmu_add_module(input_sbus
    SOURCES src/sbus_decoder.cpp src/sbus_receiver.cpp src/sbus_receiver_impl.cpp
    PUBLIC_DEPS xmotion::hal rpi_sbus Threads::Threads)

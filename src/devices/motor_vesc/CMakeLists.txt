find_package(Threads REQUIRED)

xmmu_add_module(motor_vesc
    SOURCES
        src/vesc_can_interface.cpp
        src/vesc_status_packet.cpp
        src/vesc_cmd_packet.cpp
        src/vesc_motor.cpp
    PUBLIC_DEPS xmotion::hal async_port Threads::Threads
    PUBLIC_DEFINITIONS ASIO_ENABLE_OLD_SERVICES)

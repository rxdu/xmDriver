find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Libevent REQUIRED IMPORTED_TARGET libevent)

xmmu_add_module(input_hid
    SOURCES
        # polling-based driver
        src/joystick.cpp
        src/keyboard.cpp
        # event-based driver
        src/hid_event_listener.cpp
        src/joystick_handler.cpp
        src/keyboard_handler.cpp
        src/keyboard_mapping.cpp
    PUBLIC_DEPS xmotion::hal xmotion::xmSigma Threads::Threads PkgConfig::Libevent
    PUBLIC_DEFINITIONS ASIO_ENABLE_OLD_SERVICES)

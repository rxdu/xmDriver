find_package(Threads REQUIRED)

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
    PUBLIC_DEPS xmotion::hal xmotion::xmSigma Threads::Threads)

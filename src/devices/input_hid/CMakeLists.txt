find_package(Threads REQUIRED)

# libevdev supplies the evdev parsing (device caps, abs ranges, dropped-event
# resync). Adopted in place of libevent per ADR 0002; resolved via pkg-config.
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVDEV REQUIRED IMPORTED_TARGET libevdev)

xmdriver_add_module(input_hid
    SOURCES
        src/evdev_reader.cpp
        src/evdev_translate.cpp
        src/evdev_joystick.cpp
        src/evdev_keyboard.cpp
        src/keyboard_mapping.cpp
    PUBLIC_DEPS xmotion::hal xmotion::xmBase Threads::Threads)

# libevdev is an implementation detail (not leaked through any public header),
# so link it PRIVATE — the macro exposes only PUBLIC_DEPS.
target_link_libraries(input_hid PRIVATE PkgConfig::LIBEVDEV)

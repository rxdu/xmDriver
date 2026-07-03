find_package(Threads REQUIRED)

xmdriver_add_module(motor_waveshare
    SOURCES
        src/ddsm_210.cpp
        src/ddsm_210_frame.cpp
        # sms_sts_servo.cpp #includes sms_sts_servo_impl.cpp (Impl is inline);
        # the impl is not a standalone TU.
        src/sms_sts_servo.cpp
        # vendored SCServo bus-servo library
        src/servo/SMS_STS.cpp
        src/servo/SCS.cpp
        src/servo/SCSerial.cpp
    PUBLIC_DEPS xmotion::hal xmotion::xmBase async_port Threads::Threads
    PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/servo)

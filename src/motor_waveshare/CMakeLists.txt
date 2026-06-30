if (AKELC_WITH_MODBUS)
  set(WAVESHARE_MODBUS_DEF AKELC_WITH_MODBUS)
endif ()

xmmu_add_module(motor_waveshare
    SOURCES
        src/ddsm_210.cpp
        src/ddsm_210_frame.cpp
        src/ddsm_210_array.cpp
        src/sms_sts_servo.cpp
        src/sms_sts_servo_impl.cpp
        src/sms_sts_servo_array.cpp
        # vendored SCServo bus-servo library
        src/servo/SMS_STS.cpp
        src/servo/SCS.cpp
        src/servo/SCSerial.cpp
    PUBLIC_DEPS xmotion::hal xmotion::xmSigma async_port
    PRIVATE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src/servo
    PRIVATE_DEFINITIONS ${WAVESHARE_MODBUS_DEF})

if (BUILD_APPS)
  add_subdirectory(app)
endif ()

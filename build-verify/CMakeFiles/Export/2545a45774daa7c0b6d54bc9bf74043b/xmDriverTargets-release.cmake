#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "xmotion::async_port" for configuration "Release"
set_property(TARGET xmotion::async_port APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(xmotion::async_port PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libasync_port.a"
  )

list(APPEND _cmake_import_check_targets xmotion::async_port )
list(APPEND _cmake_import_check_files_for_xmotion::async_port "${_IMPORT_PREFIX}/lib/libasync_port.a" )

# Import target "xmotion::modbus_rtu" for configuration "Release"
set_property(TARGET xmotion::modbus_rtu APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(xmotion::modbus_rtu PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libmodbus_rtu.a"
  )

list(APPEND _cmake_import_check_targets xmotion::modbus_rtu )
list(APPEND _cmake_import_check_files_for_xmotion::modbus_rtu "${_IMPORT_PREFIX}/lib/libmodbus_rtu.a" )

# Import target "xmotion::motor_akelc" for configuration "Release"
set_property(TARGET xmotion::motor_akelc APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(xmotion::motor_akelc PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libmotor_akelc.a"
  )

list(APPEND _cmake_import_check_targets xmotion::motor_akelc )
list(APPEND _cmake_import_check_files_for_xmotion::motor_akelc "${_IMPORT_PREFIX}/lib/libmotor_akelc.a" )

# Import target "xmotion::motor_vesc" for configuration "Release"
set_property(TARGET xmotion::motor_vesc APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(xmotion::motor_vesc PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libmotor_vesc.a"
  )

list(APPEND _cmake_import_check_targets xmotion::motor_vesc )
list(APPEND _cmake_import_check_files_for_xmotion::motor_vesc "${_IMPORT_PREFIX}/lib/libmotor_vesc.a" )

# Import target "xmotion::motor_waveshare" for configuration "Release"
set_property(TARGET xmotion::motor_waveshare APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(xmotion::motor_waveshare PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libmotor_waveshare.a"
  )

list(APPEND _cmake_import_check_targets xmotion::motor_waveshare )
list(APPEND _cmake_import_check_files_for_xmotion::motor_waveshare "${_IMPORT_PREFIX}/lib/libmotor_waveshare.a" )

# Import target "xmotion::motor_registry" for configuration "Release"
set_property(TARGET xmotion::motor_registry APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(xmotion::motor_registry PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libmotor_registry.a"
  )

list(APPEND _cmake_import_check_targets xmotion::motor_registry )
list(APPEND _cmake_import_check_files_for_xmotion::motor_registry "${_IMPORT_PREFIX}/lib/libmotor_registry.a" )

# Import target "xmotion::sensor_imu" for configuration "Release"
set_property(TARGET xmotion::sensor_imu APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(xmotion::sensor_imu PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C;CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libsensor_imu.a"
  )

list(APPEND _cmake_import_check_targets xmotion::sensor_imu )
list(APPEND _cmake_import_check_files_for_xmotion::sensor_imu "${_IMPORT_PREFIX}/lib/libsensor_imu.a" )

# Import target "xmotion::input_hid" for configuration "Release"
set_property(TARGET xmotion::input_hid APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(xmotion::input_hid PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libinput_hid.a"
  )

list(APPEND _cmake_import_check_targets xmotion::input_hid )
list(APPEND _cmake_import_check_files_for_xmotion::input_hid "${_IMPORT_PREFIX}/lib/libinput_hid.a" )

# Import target "xmotion::input_sbus" for configuration "Release"
set_property(TARGET xmotion::input_sbus APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(xmotion::input_sbus PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libinput_sbus.a"
  )

list(APPEND _cmake_import_check_targets xmotion::input_sbus )
list(APPEND _cmake_import_check_files_for_xmotion::input_sbus "${_IMPORT_PREFIX}/lib/libinput_sbus.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)

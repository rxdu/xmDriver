set(AKELC_WITH_MODBUS OFF)
set(AKELC_WITH_CANOPEN OFF)

if (LIBMODBUS_FOUND)
  set(AKELC_WITH_MODBUS ON)
endif ()

if (AKELC_WITH_MODBUS)
  set(AKELC_MODBUS_IMPL src/motor_akelc_modbus.cpp)
  set(AKELC_MODBUS_LIB modbus_rtu)
  set(AKELC_MODBUS_DEF AKELC_WITH_MODBUS)
endif ()

if (AKELC_WITH_MODBUS OR AKELC_WITH_CANOPEN)
  xmmu_add_module(motor_akelc
      SOURCES src/motor_akelc.cpp ${AKELC_MODBUS_IMPL}
      PUBLIC_DEPS xmotion::hal xmotion::xmSigma ${AKELC_MODBUS_LIB}
      PRIVATE_DEFINITIONS ${AKELC_MODBUS_DEF})

  if (BUILD_APPS)
    add_subdirectory(app)
  endif ()
else ()
  message(FATAL_ERROR "libmodbus not found, please install with: sudo apt-get install libmodbus-dev")
endif ()

# AKELC brushed-DC driver. LIBMODBUS_FOUND / PkgConfig::LIBMODBUS are resolved
# in src/CMakeLists.txt (parent scope).
if (LIBMODBUS_FOUND)
  xmdriver_add_module(motor_akelc
      SOURCES
          src/motor_akelc.cpp
          src/motor_akelc_modbus.cpp
      PUBLIC_DEPS xmotion::hal xmotion::xmBase modbus_rtu)
else ()
  message(FATAL_ERROR "libmodbus not found, please install with: sudo apt-get install libmodbus-dev")
endif ()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBMODBUS QUIET IMPORTED_TARGET libmodbus)

if (LIBMODBUS_FOUND)
  xmmu_add_module(modbus_rtu
      SOURCES src/modbus_rtu_port.cpp
      PUBLIC_DEPS xmotion::hal xmotion::xmSigma PkgConfig::LIBMODBUS)
else ()
  message(FATAL_ERROR "libmodbus not found, please install with: sudo apt-get install libmodbus-dev")
endif ()

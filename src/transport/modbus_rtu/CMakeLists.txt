# LIBMODBUS_FOUND / PkgConfig::LIBMODBUS are resolved in src/CMakeLists.txt.
if (LIBMODBUS_FOUND)
  xmmu_add_module(modbus_rtu
      SOURCES src/modbus_rtu_port.cpp
      # transport impl: realizes the transport interfaces, not the device HAL.
      PUBLIC_DEPS xmotion::transport_interface xmotion::xmSigma PkgConfig::LIBMODBUS)
else ()
  message(FATAL_ERROR "libmodbus not found, please install with: sudo apt-get install libmodbus-dev")
endif ()

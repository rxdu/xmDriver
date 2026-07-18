# mobile_base — the reusable Mobile-Base CAN protocol (spec: docs/can_protocol.md),
# a self-authored device protocol frozen at wire version 1.0.0. Three targets,
# layered by dependency weight:
#
#   mobile_base            header-only Core Profile codec + E2E + command tracker
#                          + model profiles. hal-free, so an embedded base / MCU
#                          shares the exact wire code (only transport_interface).
#   mobile_base_interface  abstract host-side MobileBase (mobile_base.hpp) — the
#                          robot-agnostic contract navigation depends on. Adds
#                          hal::Device on the Core Profile types; kept OUT of the
#                          MCU-clean codec above.
#   mobile_base_can_client the CAN realization (MobileBaseCanClient): opens a bus
#                          via async_can and speaks the protocol. STATIC, so only
#                          this pulls asio / async_port.
#
# Only the first uses xmdriver_add_module() (which wires the shared test/ dir);
# the other two are defined manually so test/ is not added more than once.
xmdriver_add_module(mobile_base INTERFACE
    PUBLIC_DEPS xmotion::transport_interface)

add_library(mobile_base_interface INTERFACE)
target_include_directories(mobile_base_interface INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(mobile_base_interface INTERFACE mobile_base xmotion::hal)
add_library(xmotion::mobile_base_interface ALIAS mobile_base_interface)
install(TARGETS mobile_base_interface
    EXPORT xmDriverTargets
    INCLUDES DESTINATION include)

find_package(Threads REQUIRED)
add_library(mobile_base_can_client STATIC src/mobile_base_can_client.cpp)
target_include_directories(mobile_base_can_client PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(mobile_base_can_client PUBLIC
    mobile_base_interface async_port Threads::Threads)
target_compile_definitions(mobile_base_can_client PUBLIC ASIO_ENABLE_OLD_SERVICES)
add_library(xmotion::mobile_base_can_client ALIAS mobile_base_can_client)
install(TARGETS mobile_base_can_client
    EXPORT xmDriverTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)

# mobile_base_can — the CAN realization of the Mobile-Base contract (frozen spec
# v1.0.0, docs/can_protocol.md). Serializes the vocabulary (mobile_base_types) to
# classic CAN 2.0A with an AUTOSAR-style E2E trailer. Two targets:
#
#   mobile_base_can         header-only codec + E2E + command tracker + model
#                           profiles. hal-free (types + CanFrame only), so an
#                           embedded base / MCU shares the exact wire code.
#   mobile_base_can_client  MobileBaseCanClient — the commander-side hal::Device
#                           that opens a bus via async_can and speaks the
#                           protocol. STATIC, so only this pulls asio/async_port.
#
# Only the first uses xmdriver_add_module() (which wires the shared test/ dir);
# the client is defined manually so test/ is added only once.
xmdriver_add_module(mobile_base_can INTERFACE
    PUBLIC_DEPS mobile_base_types xmotion::transport_interface)

find_package(Threads REQUIRED)
add_library(mobile_base_can_client STATIC src/mobile_base_can_client.cpp)
target_include_directories(mobile_base_can_client PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(mobile_base_can_client PUBLIC
    mobile_base_can mobile_base async_port Threads::Threads)
target_compile_definitions(mobile_base_can_client PUBLIC ASIO_ENABLE_OLD_SERVICES)
add_library(xmotion::mobile_base_can_client ALIAS mobile_base_can_client)
install(TARGETS mobile_base_can_client
    EXPORT xmDriverTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)

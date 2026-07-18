# mobile_base — the reusable Mobile-Base CAN protocol (spec: docs/can_protocol.md).
#
# A self-authored device protocol: structured like the vendor device protocols
# under src/devices/ (DDSM, VESC, SBUS), but owned by us and frozen at wire
# version 1.0.0. Header-only today — the Core profile codec, E2E integrity, the
# base-side command tracker, and opt-in model profiles (profiles/swerve_v1).
# Depends only on the first-party CanFrame at the transport boundary, so it is
# transport-agnostic and MCU-portable.
xmdriver_add_module(mobile_base INTERFACE
    PUBLIC_DEPS xmotion::transport_interface)

# mobile_base_driver — the commander-side hal::Device that opens a CAN bus and
# speaks the protocol (SetTwist / Read* / e-stop). A SEPARATE STATIC target so a
# codec-only consumer (an embedded base linking just the header-only mobile_base)
# never pulls asio / async_port. Defined manually rather than via a second
# xmdriver_add_module() so the shared test/ dir is added only once.
find_package(Threads REQUIRED)
add_library(mobile_base_driver STATIC src/mobile_base_driver.cpp)
target_include_directories(mobile_base_driver PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(mobile_base_driver PUBLIC
    mobile_base xmotion::hal async_port Threads::Threads)
target_compile_definitions(mobile_base_driver PUBLIC ASIO_ENABLE_OLD_SERVICES)
add_library(xmotion::mobile_base_driver ALIAS mobile_base_driver)
install(TARGETS mobile_base_driver
    EXPORT xmDriverTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include)

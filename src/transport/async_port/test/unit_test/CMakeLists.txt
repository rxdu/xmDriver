# Google tests
message(STATUS "Build unit tests with Google Test.")

# reference: https://cliutils.gitlab.io/modern-cmake/chapters/testing/googletest.html
mark_as_advanced(
    BUILD_GMOCK BUILD_GTEST BUILD_SHARED_LIBS
    gmock_build_tests gtest_build_samples gtest_build_tests
    gtest_disable_pthreads gtest_force_shared_crt gtest_hide_internal_symbols
)

# add unit tests. RingBuffer now lives in xmBase (xmbase/container) and is
# tested there; async_port only exercises it through the transport, covered by
# the lifecycle tests below.
add_executable(utest_async_port
    utest_async_lifecycle.cpp)
target_link_libraries(utest_async_port PRIVATE gtest gmock gtest_main async_port)
# get_target_property(PRIVATE_HEADERS imview INCLUDE_DIRECTORIES)
target_include_directories(utest_async_port PRIVATE ${PRIVATE_HEADERS})

gtest_discover_tests(utest_async_port)
add_test(NAME gtest_async_port COMMAND utest_async_port)

# Pure SocketCAN <-> CanFrame conversion logic (no hardware, no asio).
add_executable(utest_socketcan_frame utest_socketcan_frame.cpp)
target_link_libraries(utest_socketcan_frame PRIVATE gtest gmock gtest_main async_port)
gtest_discover_tests(utest_socketcan_frame)
add_test(NAME gtest_socketcan_frame COMMAND utest_socketcan_frame)

# AsyncSerial RX/TX over a pty (needs openpty from libutil).
add_executable(utest_async_loopback utest_async_loopback.cpp)
target_link_libraries(utest_async_loopback PRIVATE gtest gmock gtest_main async_port util)
gtest_discover_tests(utest_async_loopback)
add_test(NAME gtest_async_loopback COMMAND utest_async_loopback)

# AsyncCAN RX / error-callback / backpressure over an injected socketpair fd.
add_executable(utest_async_can_fd utest_async_can_fd.cpp)
target_link_libraries(utest_async_can_fd PRIVATE gtest gmock gtest_main async_port)
gtest_discover_tests(utest_async_can_fd)
add_test(NAME gtest_async_can_fd COMMAND utest_async_can_fd)
# Bundled third-party dependencies.
#
# googletest is only needed when building tests. rpi_sbus is required by the
# input_sbus driver module and is always added so that module can build.

if (BUILD_TESTS)
  add_subdirectory(googletest)
endif ()

add_subdirectory(rpi_sbus)

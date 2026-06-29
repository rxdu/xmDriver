# Bundled third-party dependencies.
#
# googletest is only needed when building tests. rpi_sbus is required by the
# input_sbus driver module and is always added so that module can build.

# Guard with NOT TARGET gtest so a superbuild that bundles several xmotion
# components (each carrying googletest) doesn't add_subdirectory it twice and
# clash on the gtest target.
if (BUILD_TESTS AND NOT TARGET gtest)
  add_subdirectory(googletest)
endif ()

# rpi_sbus installs its target into a configurable export set; route it into
# xmMu's so input_sbus (which links it) exports consistently.
set(RPI_SBUS_EXPORT_SET xmMuTargets CACHE STRING "" FORCE)
add_subdirectory(rpi_sbus)

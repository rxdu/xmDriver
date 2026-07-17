# Standalone device CLIs (hardware bring-up tools). Enabled by BUILD_APPS and
# only when xmDriver is the top-level project (see the guard in the root
# CMakeLists) — never pulled into a parent super-build that only consumes the
# libraries.
add_subdirectory(cli)

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

xmDriver is the **μ** host-hardware-driver layer of the XMotion family. It provides host-side drivers
for the physical devices on a robot:
- **Motor controllers / servos**: AKELC (Modbus), VESC (CAN), Waveshare DDSM-210 hub
  motors and SMS/STS bus servos
- **Transports**: asynchronous serial and CAN ports (`async_port`), Modbus-RTU (`modbus_rtu`)
- **Sensors**: HiPNUC serial IMU (`sensor_imu`)
- **Inputs**: joystick/keyboard HID (`input_hid`), SBUS RC receiver (`input_sbus`)

Each driver is a small static library implemented against the HAL interfaces defined here in
`src/hal`, built on the common types and logging provided by **xmBase** (`xmotion::xmBase`). The
code was extracted from the `src/driver` tree of `libxmotion` / `xmNavigation`.

## Build System

### Standard build
```bash
git submodule update --init --recursive
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

### Build with tests
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build .
ctest
```

### CMake Options
- `BUILD_TESTING`: Build tests (default: OFF)
- `XMOTION_DEV_MODE`: Development mode, forces building tests (default: OFF)
- `STATIC_CHECK`: Enable cppcheck static analysis (default: OFF)

### Dependencies

**Required:**
- CMake >= 3.10.2, C++17 compiler
- xmBase (`xmotion::xmBase`) — found via `find_package(xmBase)` or the bundled `third_party/xmBase` submodule
- System libs: `libasio-dev` (async_port/motor_vesc/sensor_imu), `libmodbus-dev` (modbus_rtu/motor_akelc),
  `libevdev-dev` (input_hid)

```bash
sudo apt-get install libasio-dev libmodbus-dev libevdev-dev
```

## Architecture

### xmBase dependency resolution
The top-level `CMakeLists.txt` resolves xmBase to a single `xmotion::xmBase` target in three
ordered steps: (1) if a parent superbuild already defines `xmotion::xmBase` in-tree (e.g. the
umbrella assembling xmBase alongside xmDriver), reuse it and add nothing; (2) otherwise try
`find_package(xmBase QUIET)` for an installed copy; (3) failing both, `add_subdirectory(third_party/xmBase)`
so xmDriver still builds standalone. Every module links the same `xmotion::xmBase` regardless of
which path resolved it.

### Module layout
- Each module lives under `src/<module>/` with `include/`, `src/`, and (most) a `test/` dir.
- Inter-module links use plain target names within the same build (e.g. `motor_vesc` links
  `async_port`; `input_sbus` links the bundled `rpi_sbus`).
- xmBase is linked via its single namespaced target `xmotion::xmBase`.

### Targets and export
- All modules are exported under `EXPORT xmDriverTargets` with `NAMESPACE xmotion::`, yielding
  `xmotion::async_port`, `xmotion::motor_vesc`, etc.
- An aggregate INTERFACE target `xmDriver` links all eight modules and is exported as `xmotion::xmDriver`
  for consumers that want everything.
- `find_package(xmDriver)` works via the generated `xmDriverConfig.cmake`, which `find_dependency(xmBase)`.

## Conventions
- **C++ Standard**: C++17. `.cpp` for sources, `.hpp` for headers. Google style, clang-format.
- **Namespace**: `xmotion`.
- Preserve per-module third-party finds (libmodbus, libevdev, asio) and the `ASIO_ENABLE_OLD_SERVICES`
  / `-fPIC` settings when editing module CMake.

## License
Apache-2.0 — see `LICENSE` and `NOTICE`. Bundled third-party components retain their own licenses.

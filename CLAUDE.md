# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

xmMu is the **μ** host-hardware-driver layer of the xMotion family. It provides host-side drivers
for the physical devices on a robot:
- **Motor controllers / servos**: AKELC (Modbus), VESC (CAN), Waveshare DDSM-210 hub
  motors and SMS/STS bus servos
- **Transports**: asynchronous serial and CAN ports (`async_port`), Modbus-RTU (`modbus_rtu`)
- **Sensors**: HiPNUC serial IMU (`sensor_imu`)
- **Inputs**: joystick/keyboard HID (`input_hid`), SBUS RC receiver (`input_sbus`)

Each driver is a small static library implemented against the interfaces defined by **xmSigma**
(`xmotion::interface`) and, where it logs, `xmotion::logging`. The code was extracted from the
`src/driver` tree of `libxmotion` / `xmNabla`.

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
- xmSigma (`xmotion-core`) — found via `find_package` or the bundled `third_party/xmSigma` submodule
- System libs: `libasio-dev` (async_port/motor_vesc/sensor_imu), `libmodbus-dev` (modbus_rtu/motor_akelc),
  `libevdev-dev` (input_hid)

```bash
sudo apt-get install libasio-dev libmodbus-dev libevdev-dev
```

## Architecture

### xmSigma dependency resolution
The top-level `CMakeLists.txt` first calls `find_package(xmotion-core QUIET)`. If that fails it
falls back to `add_subdirectory(third_party/xmSigma)`. Because the bundled submodule exposes its
targets under their plain names (`interface`, `logging`) rather than the namespaced imports created
by `find_package`, the top-level creates `xmotion::interface` / `xmotion::logging` ALIAS targets so
every module links the same namespaced names regardless of how xmSigma was resolved.

### Module layout
- Each module lives under `src/<module>/` with `include/`, `src/`, and (most) a `test/` dir.
- Inter-module links use plain target names within the same build (e.g. `motor_vesc` links
  `async_port`; `input_sbus` links the bundled `rpi_sbus`).
- xmSigma targets are linked via their namespaced names (`xmotion::interface`, `xmotion::logging`).

### Targets and export
- All modules are exported under `EXPORT xmMuTargets` with `NAMESPACE xmotion::`, yielding
  `xmotion::async_port`, `xmotion::motor_vesc`, etc.
- An aggregate INTERFACE target `xmMu` links all eight modules and is exported as `xmotion::xmMu`
  for consumers that want everything.
- `find_package(xmMu)` works via the generated `xmMuConfig.cmake`, which `find_dependency(xmotion-core)`.

## Conventions
- **C++ Standard**: C++17. `.cpp` for sources, `.hpp` for headers. Google style, clang-format.
- **Namespace**: `xmotion`.
- Preserve per-module third-party finds (libmodbus, libevdev, asio) and the `ASIO_ENABLE_OLD_SERVICES`
  / `-fPIC` settings when editing module CMake.

## License
Apache-2.0 — see `LICENSE` and `NOTICE`. Bundled third-party components retain their own licenses.

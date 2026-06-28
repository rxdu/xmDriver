<h1 align="center">
  <img src="docs/xmmu.svg" width="96" alt="xmMu"><br>
  xmMu&nbsp;·&nbsp;μ
</h1>

<p align="center"><b>Host hardware drivers for the xMotion family</b><br>
The μ layer that talks to motors, sensors and radios over serial, CAN and Modbus.</p>

---

`xmMu` is the μ hardware-driver layer of the **xMotion** product family. It provides host-side
drivers for the physical devices a robot is built from: brushed/brushless motor controllers and
servos (over serial, CAN and Modbus-RTU), inertial measurement units, and RC / human-input
devices (SBUS receivers, joysticks and keyboards). Each driver is a small, independently linkable
module built against the interfaces defined by [xmSigma](https://github.com/rxdu/xmSigma).

> Part of the xMotion family — see the [umbrella](https://github.com/rxdu/xmotion). Sibling
> components include [xmSigma](https://github.com/rxdu/xmSigma) (foundation: interfaces, logging,
> common types) and [xmNabla](https://github.com/rxdu/xmNabla) (motion algorithms).

## Modules

| Module                | Description                                                                 |
|-----------------------|-----------------------------------------------------------------------------|
| `src/async_port`      | Asynchronous serial and CAN ports (asio-based) shared by other drivers      |
| `src/modbus_rtu`      | Modbus-RTU port wrapper built on libmodbus                                   |
| `src/motor_akelc`     | AKELC motor controller driver (Modbus / CANopen transport)                  |
| `src/motor_vesc`      | VESC motor controller driver over CAN                                       |
| `src/motor_waveshare` | Waveshare DDSM-210 hub motors and SMS/STS bus servos                         |
| `src/sensor_imu`      | HiPNUC serial IMU driver                                                     |
| `src/input_hid`       | Event/polling joystick and keyboard input (libevent)                        |
| `src/input_sbus`      | SBUS RC receiver decoder and driver (bundled `rpi_sbus`)                     |

## Dependency on xmSigma

xmMu builds on **xmSigma** (CMake package name `xmotion-core`), which provides the
`xmotion::interface` and `xmotion::logging` targets. The build resolves it in one of two ways:

1. **Installed** — if `find_package(xmotion-core)` succeeds, that installation is used.
2. **Bundled** — otherwise the build falls back to the `third_party/xmSigma` submodule.

Initialise the submodules before configuring:

```bash
git submodule update --init --recursive
```

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
```

Key options: `BUILD_TESTING` (build tests, default `OFF`), `STATIC_CHECK` (cppcheck, default
`OFF`), `XMOTION_DEV_MODE` (force-build tests, default `OFF`).

### System dependencies

The driver modules link a handful of system libraries (install via apt on Ubuntu):

```bash
sudo apt-get install libasio-dev libmodbus-dev libevent-dev
```

Consumers use the aggregate target to pull in every driver module:

```cmake
find_package(xmMu REQUIRED)
target_link_libraries(my_app PRIVATE xmotion::xmMu)
```

Individual modules are also exported (e.g. `xmotion::motor_vesc`, `xmotion::sensor_imu`).

## Provenance

This code was extracted from the `src/driver` tree of `libxmotion` / `xmNabla` and repackaged as
a standalone, independently versioned component of the xMotion family.

## License

Apache-2.0 — see [LICENSE](LICENSE) and [NOTICE](NOTICE). First-party code only; bundled
third-party components (e.g. `third_party/rpi_sbus`, `third_party/googletest`) retain their own
licenses.

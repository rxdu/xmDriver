# xmDriver TODO

ADR-0003 hardening + HAL-completion effort. `[ ]` todo · `[~]` in progress · `[x]` done.

## Phase 0 — Docs & plan
- [x] ADR 0003 (HAL completion, freshness contract, transport hardening)
- [x] TODO.md + docs/LESSONS.md

## Phase 1 — HAL foundation (canonical `xmotion::hal`)
- [x] `freshness.hpp` — lock-free `FreshnessMonitor` + `HealthFromFreshness` + tests
- [x] `imu.hpp` — `hal::Imu : Device`, `Result<ImuSample>`, timestamped sample
- [x] `rc_receiver.hpp` — `hal::RcReceiver : Device`, `RcFrame{channels,frame_loss,failsafe,stamp}`
- [x] `joystick.hpp` / `keyboard.hpp` — `hal::Joystick`/`hal::Keyboard : Device` (edge-triggered hold-last-state)
- [x] `CanFrame` first-party type (un-leak `linux/can.h`) + `TransportStatus`
- [x] Remove legacy interfaces + `LegacyMotorAdapter` + their tests

## Phase 2 — Transport hardening
- [x] Shared `IoService` (one `io_context` + one I/O thread for all asio ports)
- [x] async_can: bounded TX queue + `TransportStatus` return + error callback
- [x] async_serial: `TransportStatus` return, error callback, shared io_context
- [x] Route all transport logging through the telemetry event macros (XM_*) (no cout/cerr, no frame spam)
- [x] Health signal on bus-off/unplug (error callback + drivers surface via Health)
- [x] Transport interface headers: `TransportStatus` returns, `CanFrame`
- [ ] (optional follow-up) auto-reconnect with bounded backoff — currently signal-only
      (Health + error callback); reconnection left to the supervisor layer for now

## Phase 3 — Driver migration (all to `xmotion::hal`)
- [x] VESC: freshness on reads/health; transport on `CanFrame`/`TransportStatus`
- [x] AKELC → `hal::Motor` + Speed/Brake: Result, Stop, safe Disconnect, validation, freshness
- [x] DDSM210 → `hal::Motor` + Speed/Position: non-blocking, validation, safe Disconnect, freshness
- [x] SMS-STS → `hal::Motor` + Position/Speed: `state_` bug fixed, real Health, de-blocked reads
- [x] IMU → `hal::Imu`: freshness watchdog, XM_* event logging, bounded `ch_serial` parse (OOB fixed)
- [x] input_hid joystick/keyboard → `hal::Joystick`/`Keyboard` on libevdev (races/OOB fixed)
- [x] input_sbus → `hal::RcReceiver` (first-party decoder; rpi_sbus dropped)

## Phase 4 — Build, registration, verify
- [x] `RegisterAllMotors()` aggregator module (registration not dependent on static-lib init)
- [x] Fixed vendored SCSerial `end()` fd leak
- [x] Removed `rpi_sbus` submodule (third_party + .gitmodules)
- [x] CMake: `-libevent-dev +libasio-dev +libevdev-dev`, cppcheck `--std=c++17`
- [x] Full build `-DBUILD_TESTING=ON` + `ctest` green (86/86)
- [x] Updated README module table + CLAUDE.md deps

## Phase 5 — Test-coverage hardening
- [x] SocketCAN frame conversion extracted to a header + unit-tested (EFF/RTR/dlc/id edge cases)
- [x] VescMotor made DI-testable; behavioral tests (clamp/NaN/stale/health/stop/disconnect/bus-fault)
- [x] Fixed VESC negative-current flooring bug (regen) surfaced by the new tests
- [x] AsyncSerial pty loopback test (RX + send); AsyncCAN socketpair test (RX + error + backpressure)
- [x] modbus_rtu RAII lifecycle test (construct/destruct, bogus-open, double-close, reopen)
- [x] CI: ASan+UBSan job (blocking), TSan job (advisory), coverage artifact
- [x] Full suite 123/123 green, incl. under ASan+UBSan with leak detection

## Downstream (separate effort — NOT in xmDriver)
- [ ] xmNavigation: migrate actuator groups + any IMU/RC/HID consumers to `xmotion::hal`
      (the legacy interfaces are gone; Assembly CI red until ∇ migrates — ADR 0003)

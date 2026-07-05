# ADR 0003 — HAL completion, freshness contract & transport hardening

- Status: Accepted
- Date: 2026-07-02
- Scope: all of `src/hal`, `src/transport`, `src/devices` — completes ADR 0001 across the sensor/input families and hardens the transport layer per ADR 0002.
- Related: ADR 0001 (HAL redesign — landed for the Motor family only), ADR 0002 (dependency simplification).

## Context

A review of xmMu found the ADR-0001 HAL contract (`Status`/`Result<T>`, strong units, `Device` lifecycle + `Health()`, capability mixins, config-driven `MotorFactory`) had reached only the **VESC** driver. Everything else still exhibited the four defects ADR 0001 set out to kill, plus three that cut across the whole component:

1. **No data-freshness detection** anywhere except `input_sbus`. Every other read path (including the VESC reference) serves last-known data as healthy — a silently-dead-but-connected device looks fine. This is the "dead device looks healthy" failure ADR 0001 named but only fixed for the connected/not-connected axis, not the stale axis.
2. **Errors swallowed on the command path** — AKELC discarded the `bool` from `SetSpeed`/`ApplyBrake`; `0`-on-read-error made a bus fault indistinguishable from a real zero.
3. **Unsafe teardown** — DDSM210 `Disconnect()` closed the port without commanding a safe state; AKELC had no lifecycle at all.
4. **Observability gap** — transport (`async_serial`/`async_can`) logged via `std::cout`/`std::cerr`; `sensor_imu` and the polling joystick logged nothing.
5. **Performance/determinism** — thread-per-port (contrary to ADR 0002's shared `io_context` decision); an unbounded CAN TX queue; blocking sleeps on Waveshare command/read paths.
6. **Integration leaks** — sensor/input interfaces still legacy (throwing `SensorInterface<T>`, `Connect(dev, baud)` baking in serial, inconsistent lifecycle vocab); `CanInterface` exposing `linux/can.h`.

## Decision

Complete the ADR-0001 contract across **every** device family, add a uniform freshness primitive, and harden the transport. Legacy interfaces are **removed, not shimmed** — the new `xmotion::hal` contract is the single canonical interface. Upper layers (xmNabla, apps) migrate to it next; the umbrella Assembly CI is expected red until they do. This keeps xmMu free of compatibility duplication.

### 1. Freshness is a first-class HAL primitive

A new lock-free `hal::FreshnessMonitor` (`freshness.hpp`, `std::chrono::steady_clock`, an `atomic` timestamp — no lock on the hot path) is the single mechanism for staleness:

- A driver calls `Mark()` on every successful device update (RX frame, poll, feedback).
- Reads consult it: a read with no fresh sample returns `Result<T>::Err(kTimeout)` (never a stale value dressed as `kOk`).
- `Health()` maps `!IsFresh()` while connected to `State::kDegraded` (data flowing but late) or `kFault` (link presumed lost), with `detail` carrying the age.

Sensor sample structs carry a `steady_clock::time_point stamp`, so consumers that fuse data can reason about age directly. This makes staleness impossible to forget: it is part of the read/health contract, not per-driver discipline. `input_sbus`'s existing 200 ms watchdog is the reference behaviour, generalised.

### 2. Sensor/input families adopt the ADR-0001 contract

New canonical interfaces in `xmotion::hal`, all deriving `Device` (uniform `Connect`/`Disconnect`/`IsConnected`/`Health`), reads returning `Result<T>`, no throwing for "wrong transport", transport params supplied at construction:

- `hal::Imu` — `Result<ImuSample> Read()` + callback; `ImuSample` timestamped.
- `hal::RcReceiver` — `Result<RcFrame> Read()` + callback; `RcFrame{channels, frame_loss, failsafe, stamp}`.
- `hal::Joystick`, `hal::Keyboard` — `Device` + polled state/`Result` reads + event callbacks; explicit connected/health so a hot-unplug is observable.

Removed: `imu_interface.hpp`, `sensor_interface.hpp`, `joystick_interface.hpp`, `keyboard_interface.hpp`, `rc_receiver_interface.hpp`, `hid_handler_interface.hpp`, `motor_controller_interface.hpp`, `motor_controller_array_interface.hpp`, `compat/legacy_motor_adapter.hpp` (and their tests).

### 3. Transport hardening

- **One shared `io_context` + one I/O thread** for all asio ports (ADR 0002 decision), via a process-wide `hal`-agnostic `IoService` the ports post onto. Replaces thread-per-port.
- **Bounded TX** on CAN (matching serial's bounded ring); `SendFrame`/`SendBytes` return `Status` (or `Result`) so the caller sees backpressure/"queue full"/"not connected" instead of `void`.
- **Structured logging** — all transport diagnostics via the xmBase telemetry event macros (`XM_*`; originally XLOG, migrated in the v0.3.0 telemetry clean break); no `std::cout`/`std::cerr`, no per-frame stdout spam.
- **Health + recovery** — ports expose health; on bus-off/unplug they surface a health state (auto-reconnect with bounded backoff is provided where safe, otherwise the app is signalled — never a silent dead port).
- **Un-leak the OS** — `struct can_frame` (`linux/can.h`) no longer appears in `can_interface.hpp`; a first-party `hal`/transport `CanFrame` is used at the boundary.

### 4. Driver correctness (folded into migration)

Every driver migrated to the contract also gets: NaN/inf rejection + clamp-and-report on commands; a real `Stop()` safe state; `Disconnect()` that commands the safe state before closing; real `Health()` (no hardcoded `true`); reads via `Result<T>` + freshness. Specific fixes: SMS-STS `GetStates()` member-vs-local state bug; DDSM210 blocking `SetMode`/`SetMotorId` and unsafe `Disconnect`; SMS-STS blocking per-servo `FeedBack` sleeps moved off the read path; `ch_serial` bounded parsing.

### 5. Driver registration robustness

Config-driven construction must not depend on a static-lib initializer the linker can garbage-collect. Each device family exposes an explicit `RegisterXxxDrivers()` aggregator; an app (or an `xmMu` convenience `RegisterAllDrivers()`) calls it. Static self-registration is kept as a best-effort convenience for whole-archive linking, not the contract.

## Consequences

- Single canonical HAL; no legacy/compat duplication in xmMu.
- **Breaking for consumers**: xmNabla and apps that used the legacy interfaces must migrate to `xmotion::hal`. Assembly CI (Σ+μ+∇) is red until ∇ migrates — accepted, sequenced deliberately (μ first, then ∇).
- Packaging/build: `libevent-dev` removed, `libasio-dev` declared, `libevdev-dev` added (per ADR 0002); cppcheck std corrected to c++17.
- One async model, one I/O thread, bounded memory, structured logs, uniform freshness/health — the four review axes (reliable, high-performance, observable, integrable) addressed at the contract level rather than per-driver.

## Verification

Each block is verified by `-DBUILD_TESTING=ON` + `ctest` in xmMu. New tests cover: `FreshnessMonitor` staleness transitions; each migrated driver's clamp/NaN/Stop/Disconnect-safe-state/stale-read behaviour against a fake transport; bounded-TX backpressure. The umbrella Assembly job is re-pinned only after xmNabla migrates.

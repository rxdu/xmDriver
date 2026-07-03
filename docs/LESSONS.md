# xmDriver Lessons

Team-visible record of mistakes and the patterns that replace them. Consult during intake and after errors.

### A partial HAL migration poisons the whole abstraction
- **Pattern:** ADR 0001's HAL contract was landed for one driver (VESC) and treated as "done". The other seven drivers kept legacy behaviour, so upper layers that `dynamic_cast` to a capability mixin and call it uniformly still hit stale reads, swallowed errors and missing failsafes — the abstraction was only as strong as its weakest driver.
- **Correction:** A HAL contract is done when *every* implementation satisfies it. Migrate breadth-first (all drivers to the new contract) before declaring the layer stable; gate on a conformance checklist derived from the ADR.
- **Context:** xmDriver / C++ HAL design.

### "Connected" is not "fresh" — stale data must be detectable
- **Pattern:** Every read path except SBUS returned last-known data as `kOk`. A device that was connected but had stopped sending (unplugged mid-run, bus-off, sensor hang) looked perfectly healthy. `Result<T>`/`IsConnected()` only distinguished connected-vs-not, never live-vs-stale.
- **Correction:** Make freshness a first-class, uniform primitive (`FreshnessMonitor`): `Mark()` on every successful update; reads with no fresh sample return `kTimeout`; `Health()` degrades on staleness. Sample structs carry a monotonic timestamp. Never dress a stale value as `kOk`.
- **Context:** Robotics driver reliability — applies to any sensor/actuator read path.

### Command-path errors must propagate, teardown must reach a safe state
- **Pattern:** AKELC discarded the `bool` returned by `SetSpeed`/`ApplyBrake` — a Modbus write timeout on a brake command was silent. DDSM210 `Disconnect()` closed the serial port without commanding zero speed, leaving the motor spinning unless the destructor happened to run.
- **Correction:** Every command returns `Status`; callers propagate it. `Disconnect()` commands the failsafe (`Stop()`) *before* closing transport, matching the VESC reference. Never rely solely on a destructor for a safe state.
- **Context:** Robotics anti-patterns #2 (unapproved behaviour), #3 (hiding failures).

### An ADR decision is not implemented until the code reflects it
- **Pattern:** ADR 0002 decided on one shared asio `io_context` with a single I/O thread; the code kept thread-per-port. The CAN TX path was left unbounded despite the determinism requirement.
- **Correction:** When an ADR records a decision with runtime consequences (threading, bounds, allocation), land the code change in the same or an immediately-following PR, and note the deviation in the ADR status if it must lag.
- **Context:** xmDriver transport / asio.

### Use the project logger at every boundary
- **Pattern:** The transport layer — the most failure-prone code — logged via `std::cout`/`std::cerr` (no levels, no timestamps, unfilterable) and even printed every received CAN frame to stdout by default. Several drivers logged nothing at all.
- **Correction:** Route all connect/disconnect/fault/timeout/reconnect diagnostics through the xmBase logger (XLOG). No `std::cout`/`std::cerr` in library code; no unconditional per-frame prints.
- **Context:** Observability — xmDriver.

### A silent clamp that changes behavior is a hidden safety bug — and a test seam finds it
- **Pattern:** `VescSetCurrentCmdPacket` clamped motor current to `[0, 20]` A, silently flooring any negative (regen/braking) command to `0` — directly contradicting the driver's documented contract that negative current is regen. It was invisible because nothing exercised the negative path with a transport it could observe.
- **Correction:** Clamp to the true signed range (`[-20, 20]`), enforce the app envelope upstream, and never let a bound silently reinterpret a command (clamp-and-report, not clamp-and-hide). Give each driver a dependency-injection seam (a fake transport) so command→wire behavior is asserted, not assumed — that seam is what surfaced this.
- **Context:** Robotics anti-patterns #1 (silent physical-world assumptions) and #2 (unapproved behavior change); xmDriver / VESC.

### Make transport testable: extract pure logic, add a loopback seam
- **Pattern:** The transport layer was rewritten in place with no test seam, so it had only lifecycle smoke tests while the drivers (which had fake-transport seams) were thoroughly covered. The error-prone SocketCAN bit-twiddling (EFF/RTR flags, dlc clamp, id masking) was untested.
- **Correction:** Extract pure conversion logic into a header so it unit-tests directly; add a loopback seam (pty for serial, socketpair + an internal `OpenFd` for CAN) so RX, error-callback, and backpressure paths run without hardware. Coverage shape follows testability seams — build them in.
- **Context:** xmDriver transport; testing.

# ADR 0002 — Dependency review & simplification

- Status: Proposed
- Date: 2026-06-30
- Scope: xmMu external/bundled dependencies and the transport layer that uses them
- Related: ADR 0001 (HAL redesign — the `xmmu::transport` tier these libs sit behind)

## Context

A review of xmMu's dependencies asked whether each is still the best choice and whether better libraries could simplify the implementation. Current dependencies:

| Dependency | Kind | Used by |
|------------|------|---------|
| asio (standalone) | system (`libasio-dev`) | async_port (serial + CAN), sensor_imu |
| libmodbus | system (`libmodbus-dev`) | modbus_rtu, motor_akelc |
| libevent | system (`libevent-dev`) | input_hid |
| Eigen | system (`libeigen3-dev`, via xmSigma) | types |
| spdlog | system (via xmSigma) | logging |
| rpi_sbus | bundled (`third_party`) | input_sbus |
| SCServo | vendored (`src/servo`) | motor_waveshare |
| ch_serial | vendored (`src`) | sensor_imu |
| GoogleTest | bundled | tests |

The key finding: the library *choices* are mostly sound; the pain surfaced by the code review is **usage** (hand-rolled async around asio) and **redundancy** (two event-loop frameworks; a bundled library for a trivial protocol), not bad libraries. So "better libraries" mostly means *fewer* libraries used correctly, not swaps.

## Decision

### Keep

- **asio (standalone)** for serial *and* CAN. It is the de-facto C++ async-I/O library; standalone asio (`chriskohlhoff/asio`) is header-only and already packaged. The C++ Networking TS was not merged into the standard and `std::execution` (P2300) is not an I/O library, so there is no standard replacement as of 2026. The review's CAN defects (self-join, dangling send buffer, cross-thread descriptor race) are **asio-usage** bugs, fixed in ADR-0001's transport hardening, not reasons to switch:
  - one shared `io_context` with a single I/O thread serving every port (replacing thread-per-port);
  - all socket operations issued on that thread (via `post`/a `strand`) so a single descriptor never has overlapping ops from two threads — asio's documented requirement, met by a strand;
  - send buffers owned for the lifetime of the async write; `Close()` never called from a completion handler.
  - *Alternative considered:* a bespoke SocketCAN reader thread (drop asio for CAN). Rejected — it removes no dependency (asio stays for serial) and reintroduces the hand-rolled concurrency that caused the bugs in the first place.
- **libmodbus** — still the standard Modbus-RTU library; no better C/C++ option. Wrap the `modbus_t*` context in an RAII type to fix the leak / double-free / un-nulled-context defects (a usage fix, not a swap).
- **Eigen, spdlog, GoogleTest** — standard; unchanged.
- **SCServo, ch_serial** (vendored, hardware-specific protocols) — no drop-in alternatives. Keep, isolate behind `xmmu::transport`/HAL, and harden the unsafe spots (e.g. the `ch_serial` OOB read) rather than rewrite.

### Drop

- **libevent** → removed. It is a *second* event-loop framework pulled in only to watch a couple of evdev fds, while asio already provides an event loop; it caused the HID leaks/races. HID device readiness moves onto the shared asio `io_context` (a posix descriptor `async_wait`) or a small epoll loop.
- **rpi_sbus** (bundled) → removed. SBUS decoding is a fixed 25-byte frame unpack (~30 lines), and the repo already contains a first-party `SbusDecoder` that is currently dead code. Consolidate on one first-party decoder and delete the duplicate + the bundled library.

### Add

- **libevdev** (`libevdev-dev`) for evdev parsing in input_hid — the purpose-built library for reading joystick/keyboard devices. It replaces the manual `input_event` parsing with correct handling the current code lacks. Verified integration model (sources below):
  - the **caller owns the fd**: `fd = open(dev, O_RDONLY | O_NONBLOCK)` → `libevdev_new_from_fd(fd, &dev)`; `libevdev_free` + `close(fd)` on teardown;
  - **own event loop**: `poll()`/asio `async_wait` the fd for readiness, then drain with `libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)` while it returns `LIBEVDEV_READ_STATUS_SUCCESS` (0); `-EAGAIN` means drained;
  - **dropped-event resync**: on `LIBEVDEV_READ_STATUS_SYNC` (a `SYN_DROPPED`), loop with `LIBEVDEV_READ_FLAG_SYNC` to replay the state delta — the resync the current driver does not do;
  - **axis ranges** via `libevdev_get_abs_info()` / `libevdev_get_abs_minimum/maximum()` — removes the hand-rolled min/max and the divide-by-zero the review found;
  - one `libevdev` handle per device (no shared mutable state across devices), matching a one-device-per-thread or single-io-thread model.

### Net effect

From **asio + libmodbus + libevent (system) + rpi_sbus (bundled)** to **asio + libmodbus + libevdev (system)** for the I/O-facing layer: one fewer event-loop framework, one fewer bundled library, the right tool for HID, and a single async model (asio) across the component. The change is correctness/right-tool driven, not merely a count reduction (libevent→libevdev is a swap; rpi_sbus is a net removal).

## Verification

- libevdev event model, `SYN_DROPPED`/`LIBEVDEV_READ_FLAG_SYNC` resync, caller-owned fd, non-blocking drain: confirmed from the libevdev project docs and the maintainer's reference write-up (sources below).
- asio single-`io_context` event-loop model and `strand` serialization (the basis for fixing the CAN descriptor race) confirmed from the standalone asio docs.

Sources:
- libevdev SYN_DROPPED handling — <https://www.freedesktop.org/software/libevdev/doc/latest/syn_dropped.html>
- libevdev event handling API — <https://www.freedesktop.org/software/libevdev/doc/latest/group__events.html>
- libevdev usage (maintainer) — <http://who-t.blogspot.com/2013/09/libevdev-handling-input-events.html>
- asio `io_context` / `strand` — <https://github.com/chriskohlhoff/asio>

## Migration plan (sequenced; each its own PR, CI-verified)

These are breaking/cross-cutting and touch transports every driver uses, so they land **after** the ADR-0001 `async_port` hardening and **behind** the `xmmu::transport` tier, so the library choice stays an implementation detail.

1. **async_port hardening** (ADR-0001): one shared `io_context`, strand-serialized sends, owned send buffers, no self-join. (Prerequisite — fixes asio usage.)
2. **Drop rpi_sbus**: single first-party SBUS decoder, delete the dead duplicate and the bundled lib. (Smallest, self-contained.)
3. **Drop libevent**: move input_hid onto asio (shared `io_context`) and adopt **libevdev** for parsing; update CI/packaging deps (`-libevent-dev +libevdev-dev`).
4. **libmodbus RAII wrapper** for `modbus_rtu` (fixes the context lifecycle defects).
5. Harden the vendored `ch_serial` / SCServo edges behind the transport/HAL.

Each step is verified by xmMu CI (`-DBUILD_TESTING=ON` + `ctest`) and the umbrella Assembly CI (Σ+μ+∇) on re-pin.

## Consequences

- One async model (asio) across the component; HID and SBUS become dependency-light and more correct.
- Packaging/CI dependency set changes: `libevent-dev` removed, `libevdev-dev` added; the bundled `rpi_sbus` submodule removed.
- Vendored hardware protocols remain, intentionally — isolated and hardened, not swapped.

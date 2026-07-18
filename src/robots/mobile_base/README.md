# mobile_base — Mobile-Base CAN protocol

A reusable, production-grade CAN interface between an upper computer ("commander") and a mobile robot **base**. To the commander the base is one CAN device: send a **body twist** plus mode / e-stop, read back status, odometry, capabilities, limits, battery, and faults. The protocol is **base-type agnostic** — one mandatory **Core Profile** serves differential, Ackermann, omni, mecanum, and swerve bases; base-specific data rides in opt-in **Model Profile** extension frames.

This is a self-authored *device protocol*, sitting under `src/robots/` alongside the vendor device protocols in `src/devices/` (DDSM, VESC, SBUS) — the difference is we own and freeze it. Wire version **1.0.0**.

- **Normative spec:** [`docs/can_protocol.md`](docs/can_protocol.md) (frozen v1.0.0)
- **Machine-readable dictionary:** [`docs/mobile_base.dbc`](docs/mobile_base.dbc) (CANalyzer / SavvyCAN / codegen)

## Two targets, by dependency weight

| Target | Kind | Depends on | Who links it |
|---|---|---|---|
| `xmotion::mobile_base` | header-only | `transport_interface` (just `CanFrame`) | the **base** side, or any embedded/MCU consumer — no asio |
| `xmotion::mobile_base_driver` | static lib | `mobile_base`, `hal`, `async_port` | the **commander** (autonomy) — opens a real CAN bus |

Keeping the codec header-only and asio-free means the base can share the exact same encode/decode/E2E code as the commander without inheriting the transport stack.

## Integrity (E2E, spec §4)

Every frame carries a uniform trailer: **byte 6 = alive counter, byte 7 = CRC-8/AUTOSAR** over the CAN id + payload. This gives corruption detection, **masquerade** protection (a frame replayed under another id fails the CRC), and **replay/gap** detection (the alive counter). `Decode*()` enforces the CRC before it ever returns a value; `CounterCheck` validates ordering.

## Commander side

```cpp
#include "mobile_base/mobile_base_driver.hpp"
using namespace xmotion;

MobileBase base({.can_interface = "can0"});
if (base.Connect() != hal::Status::kOk) { /* handle */ }

base.SetMode(mobile_base::ModeRequest::kAuto);
base.SetTwist(/*vx*/ 0.5, /*vy*/ 0.0, /*wz*/ 0.3);   // m/s, m/s, rad/s

if (auto odom = base.ReadOdomTwist(); odom.ok())      // kTimeout if base went silent
  use(odom.value.vx, odom.value.wz);

base.EngageEStop();                                   // ClearEStop() re-arms with the key
```

Reads return `hal::Result<T>` — a silent (disconnected or stalled) base yields `kTimeout`, never a stale value read back as fresh. Model-profile frames are delivered verbatim via `SetModelFrameCallback` so a profile layer decodes them; the driver stays model-agnostic.

## Base side

The base uses the header-only codec plus `CommandTracker`, which folds the three safety gates into one call:

```cpp
#include "mobile_base/command_tracker.hpp"
using namespace xmotion::mobile_base;

CommandTracker tracker;                       // integrity + version + freshness
// on each received frame:
tracker.OnFrame(rx, CommandTracker::Clock::now());
// each control cycle:
auto cmd = tracker.Snapshot(CommandTracker::Clock::now());
if (cmd.twist_fresh && !cmd.estop_engaged)
  drive(cmd.vx, cmd.vy, cmd.wz);              // stale/untrusted => zero, by construction
```

## Adding a model profile

Pick ids from the reserved extension ranges (`0x120–0x17F` command, `0x220–0x27F` state), build frames with the public helpers (`MakeFrame` / `PutI16` / `StampE2e`), and advertise `model_profile_id` / `_ver` in `STATE_CAPABILITIES`. The core never switches on the profile, so a new base type needs **no core change**. See [`include/mobile_base/profiles/swerve_v1.hpp`](include/mobile_base/profiles/swerve_v1.hpp) for the reference.

## Versioning

Three independent axes (spec §1.4): the **protocol version byte** (on the wire, = spec major, stable), each **model profile version**, and the library version. Evolving a profile never bumps the core.

## Tests

Hardware-free (`ctest -R robots::mobile_base`): E2E integrity, core-codec round-trips + reject paths, the command tracker's gates, the swerve profile, a DBC↔codec sync check, and the driver over a fake bus.

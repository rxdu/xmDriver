# mobile_base_can — CAN realization of the Mobile-Base contract

The **CAN** realization of the abstract [`MobileBase`](../mobile_base/include/mobile_base/mobile_base.hpp) contract (defined in the sibling `mobile_base/` module): a reusable, production-grade interface between an upper computer ("commander") and a mobile robot **base**. Send a **body twist** plus mode / e-stop; read back status, odometry, capabilities, limits, battery, and faults. **Base-type agnostic** — one mandatory **Core Profile** serves differential, Ackermann, omni, mecanum, and swerve bases; base-specific data rides in opt-in **Model Profile** extension frames.

A self-authored *device protocol* under `src/robots/`, alongside the vendor device protocols in `src/devices/` (DDSM, VESC, SBUS) — we own and freeze it. Wire version **1.0.0**.

- **Normative spec:** [`docs/can_protocol.md`](docs/can_protocol.md) (frozen v1.0.0)
- **Machine-readable dictionary:** [`docs/mobile_base.dbc`](docs/mobile_base.dbc) (CANalyzer / SavvyCAN / codegen)

## Where this fits — contract vs realizations

Navigation depends on the abstract **`MobileBase`** (in `mobile_base/`) — body twist in, odometry + capabilities out — never on a robot's protocol or wiring. Each robot provides an implementation, so adding a base is a new class, not a framework change.

| Target | Kind | Depends on | Who links it |
|---|---|---|---|
| `xmotion::mobile_base_types` | header-only | — (stdlib) | the SI vocabulary; any base / MCU / impl |
| `xmotion::mobile_base` | header-only | `mobile_base_types`, `hal` | **navigation** — the abstract `MobileBase` |
| `xmotion::mobile_base_can` | header-only | `mobile_base_types`, `transport_interface` | the **base** side / MCU — codec, hal-free, no asio |
| `xmotion::mobile_base_can_client` | static lib | `mobile_base_can`, `mobile_base`, `async_port` | the **commander** — `MobileBaseCanClient` |

The vocabulary and codec stay hal/asio-free so a base (or an MCU) shares the exact encode/decode/E2E code; the interface adds `hal::Device`; only the client pulls asio.

Implementations of `MobileBase`: **`MobileBaseCanClient`** (here — serializes the Core Profile over CAN to a firmware base like swervebot), and a planned **host-composed** base ([`../ackermann_vesc`](../ackermann_vesc/DESIGN.md) — an RC car mapping twist → a bicycle model → VESC + steering servo, no wire protocol).

## Integrity (E2E, spec §4)

Every frame carries a uniform trailer: **byte 6 = alive counter, byte 7 = CRC-8/AUTOSAR** over the CAN id + payload. This gives corruption detection, **masquerade** protection (a frame replayed under another id fails the CRC), and **replay/gap** detection (the alive counter). `Decode*()` enforces the CRC before it ever returns a value; `CounterCheck` validates ordering.

## Commander side

```cpp
#include "mobile_base_can/mobile_base_can_client.hpp"
using namespace xmotion;

MobileBaseCanClient base({.can_interface = "can0"});
if (base.Connect() != hal::Status::kOk) { /* handle */ }

base.SetMode(mobile_base::ModeRequest::kAuto);
base.SetTwist(/*vx*/ 0.5, /*vy*/ 0.0, /*wz*/ 0.3);   // m/s, m/s, rad/s

if (auto odom = base.ReadOdomTwist(); odom.ok())      // kTimeout if base went silent
  use(odom.value.vx, odom.value.wz);

base.EngageEStop();                                   // ClearEStop() re-arms with the key
```

Navigation holds it as the abstract `MobileBase&` and never sees the CAN specifics. Reads return `hal::Result<T>` — a silent (disconnected or stalled) base yields `kTimeout`, never a stale value read back as fresh. Model-profile frames are delivered verbatim via `MobileBaseCanClient::SetModelFrameCallback` (a client extra, outside the abstract contract) so a profile layer decodes them.

## Base side

The base uses the header-only codec plus `CommandTracker`, which folds the three safety gates into one call:

```cpp
#include "mobile_base_can/command_tracker.hpp"
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

Pick ids from the reserved extension ranges (`0x120–0x17F` command, `0x220–0x27F` state), build frames with the public helpers (`MakeFrame` / `PutI16` / `StampE2e`), and advertise `model_profile_id` / `_ver` in `STATE_CAPABILITIES`. The core never switches on the profile, so a new base type needs **no core change**. See [`include/mobile_base_can/profiles/swerve_v1.hpp`](include/mobile_base_can/profiles/swerve_v1.hpp) for the reference.

## Versioning

Three independent axes (spec §1.4): the **protocol version byte** (on the wire, = spec major, stable), each **model profile version**, and the library version. Evolving a profile never bumps the core.

## Tests

Hardware-free (`ctest -R robots::mobile_base`): E2E integrity, core-codec round-trips + reject paths, the command tracker's gates, the swerve profile, a DBC↔codec sync check, and the driver over a fake bus.

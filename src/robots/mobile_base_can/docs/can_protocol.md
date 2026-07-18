# Mobile Base CAN Protocol Specification

- **Spec version: 1.0.0** — frozen. Changes after this follow the versioning rules of §1.4.
- **Protocol version byte: 1** — carried on the wire; equals the spec major version.
- **Core Profile: Mobile Base v1.** **Model Profiles:** Swerve Module v1 (§8.1).
- Home: **xmDriver** — CAN realization at `src/robots/mobile_base_can/`; the abstract contract (`xmotion::MobileBase` + SI vocabulary) at the sibling `src/robots/mobile_base/`. A base implements the contract; the commander-side `MobileBaseCanClient` speaks this wire protocol. Any XMotion base reuses it by vendoring xmDriver.
- Reference implementation: `include/mobile_base_can/` — `core_codec.hpp` (codec), `e2e.hpp` (integrity), `command_tracker.hpp` (base-side gates), model profiles under `profiles/`; the SI vocabulary is `../mobile_base/include/mobile_base/types.hpp`. Machine-readable dictionary: `docs/mobile_base.dbc` (§13). Conformance tests: `test/`.

The key words MUST, MUST NOT, REQUIRED, SHALL, SHOULD, MAY are to be interpreted as in RFC 2119.

---

## 1. Scope, roles, and versioning

### 1.1 Purpose

This is the byte-level specification of the CAN interface exposed by a **mobile robot base** to an upper computer ("commander"). To the commander, the base is a CAN device like any commercial chassis: it accepts a **body-twist** command plus mode/e-stop control, and reports state, odometry, capabilities, faults, and battery. The protocol is **base-type agnostic** — the mandatory **Core Profile** applies to differential, Ackermann, omni, mecanum, and swerve bases alike; base-specific data lives in **Model Profile** extension frames (§8). Everything the two computers exchange goes through the frames defined here; there is no side channel.

### 1.2 Bus and encoding

- **Bus**: classic CAN 2.0A (8-byte data frames, standard 11-bit identifiers). Extended (29-bit) IDs and CAN FD are not used by the core (see Appendix A for an optional multi-base 29-bit mapping). Default bitrate **500 kbit/s** (§2).
- **Byte order**: all multi-byte fields are **little-endian**.
- **Node roles**: exactly one **commander** and one **base** on the core mapping. The core is not designed for multiple commanders; a second commander's arbitration is out of scope (last valid command wins). Multiple bases on one bus use Appendix A.
- **DLC rule**: every frame defined here is sent with **DLC 8**. Unused payload bytes are reserved, MUST be written as zero, and MUST be ignored on read (minor-version expansion space). A receiver MUST reject (ignore, count as malformed per §10) any defined frame ID whose DLC ≠ 8.
- **Every frame carries the E2E trailer** of §4: **byte 6 = alive counter, byte 7 = CRC-8**. Payload therefore occupies bytes 0–5.
- **Unknown IDs**: frames with IDs not defined by the base's active profiles MUST be ignored silently.

### 1.3 Profiles

- **Core Mobile Base Profile v1** — mandatory. Twist/mode/e-stop/heartbeat commands; status, odometry, capabilities, limits, battery, faults. Defined in §6–§7.
- **Model Profiles** — optional extensions carrying base-type-specific data (per-module state, per-wheel encoders, steering feedback, …), in the reserved extension ID ranges (§3). A base advertises its model profile in `STATE_CAPABILITIES` (§7.4). This document defines **Swerve Module v1** (§8.1); other bases register their own.

### 1.4 Versioning

Semantic versioning, three independent axes:

- **Spec version** (this document). Major/minor/patch as below.
- **Protocol version byte** (`CMD_HEARTBEAT`, `STATE_STATUS`, `STATE_CAPABILITIES`) — equals the **spec major** version. A cross-major peer cannot safely interoperate.
- **Model profile version** (`STATE_CAPABILITIES`) — versions a model profile independently of the core.

Change classes: **Major** — anything an existing peer could misinterpret (ID reassignment, field moves, scaling or E2E changes, semantic changes to failsafe). **Minor** — additions that only consume reserved bytes, reserved IDs, or new model profiles; existing peers stay correct. **Patch** — clarifications with no byte-level effect.

A base receiving a `CMD_HEARTBEAT` whose version byte it does not implement MUST suspend command processing (§9.4) and raise `FAULT_PROTO_VERSION`. A commander receiving a `STATE_STATUS` with an unknown version byte MUST NOT command motion.

---

## 2. Physical layer (informative)

Not part of the logical contract, but recommended for interoperable reference builds:

- **Bitrate 500 kbit/s**, sample point **87.5 %** (CiA 601-3), SJW 1–2 TQ.
- **Termination 120 Ω** at both physical ends of the bus; nowhere else.
- Linear bus topology; stubs ≤ 0.3 m; total length ≤ 40 m at 500 kbit/s.
- Isolated transceivers recommended; a common ground reference between nodes is REQUIRED.
- The bus is assumed **trusted** — this protocol provides integrity (§4), not authentication. If authentication is required, layer it separately (e.g. AUTOSAR SecOC); it is out of scope here.

---

## 3. Frame ID map

Command frames (commander → base) occupy `0x100`–`0x1FF`; state frames (base → commander) occupy `0x200`–`0x2FF`. **Lower ID = higher bus priority**, so safety-critical frames carry the lowest IDs of their block. Each block is partitioned:

| Range | Contents |
|---|---|
| `0x100`–`0x11F` | Core commands |
| `0x120`–`0x17F` | Model command extensions |
| `0x180`–`0x1FF` | Reserved (future core / vendor) |
| `0x200`–`0x21F` | Core state |
| `0x220`–`0x27F` | Model state extensions |
| `0x280`–`0x2FF` | Reserved (future core / vendor) |

| ID | Name | Dir | Nominal rate | Profile | Purpose |
|---|---|---|---|---|---|
| 0x100 | `CMD_TWIST` | C→B | 50 Hz | core | body-twist setpoint |
| 0x101 | `CMD_MODE` | C→B | on change | core | request standby/auto |
| 0x102 | `CMD_ESTOP` | C→B | on demand | core | e-stop engage/clear |
| 0x103 | `CMD_HEARTBEAT` | C→B | 10 Hz | core | link liveness + version |
| 0x200 | `STATE_STATUS` | B→C | 10 Hz | core | mode/flags/faults/version (**base heartbeat**) |
| 0x201 | `STATE_ODOM_TWIST` | B→C | 50 Hz | core | measured body twist |
| 0x202 | `STATE_ODOM_POSE` | B→C | 50 Hz | core | body-frame pose delta |
| 0x203 | `STATE_CAPABILITIES` | B→C | 1 Hz + on request | core | base type, DOF, profile ids |
| 0x204 | `STATE_LIMITS` | B→C | 1 Hz | core | max vx/vy/wz |
| 0x205 | `STATE_BATTERY` | B→C | 1 Hz | core | pack voltage/current/SoC |
| 0x206 | `STATE_FAULTS` | B→C | 1 Hz + on change | core | full fault word |
| 0x220+i | `STATE_MODULE[i]`, i∈0..3 | B→C | 20 Hz/module | swerve v1 | swerve module state |

Rates are configuration defaults (`config/sbot.yaml`, `can.rates.*`). The command rates are the commander's obligation. The **timeout-failsafe** (§9.1) and **liveness** (§9.5) are defined against configured timeouts, not against these nominal rates.

---

## 4. Message integrity (end-to-end)

Every frame carries an AUTOSAR-E2E-style trailer so a receiver can distinguish an authentic, fresh, in-order frame from a corrupted, duplicated, stale, or masquerading one — beyond what CAN's own CRC (which protects only the wire) provides.

| Byte | Field | Purpose |
|---|---|---|
| 6 | `counter` | alive counter, `+1` per transmission of that frame ID, wraps 255→0 |
| 7 | `crc` | CRC-8 over the Data-ID and bytes 0–6 |

**CRC.** `crc = CRC8( [id_lo, id_hi, data0..data6] )` where `id_lo/id_hi` are the frame's 11-bit CAN ID as little-endian u16 (the per-message **Data-ID**, giving masquerade protection), and `CRC8` is **CRC-8/AUTOSAR** (polynomial `0x2F`, init `0xFF`, RefIn/RefOut false, XorOut `0xFF`). A receiver MUST recompute the CRC using the **received frame's ID** and reject a mismatch as malformed (§10, `FAULT_MALFORMED_FRAME`).

**Alive counter.** A receiver tracks the last accepted counter per frame ID. The first frame after boot/reset initialises the reference and is accepted. A subsequent frame is **in-order** iff `(counter − last) mod 256 ∈ [1, MaxDelta]` (default `MaxDelta = 15`, tolerating ≤14 consecutive losses). A **stuck** counter (`delta = 0`, i.e. a duplicate/replay) or a **gap** larger than `MaxDelta` is a sequence error.

**Control gating.** On **command** frames, a CRC failure or a sequence error MUST NOT update control state (the command is dropped and does not refresh freshness, §9.1) and raises `FAULT_CMD_INTEGRITY`. On **state** frames the same checks let the commander detect a degraded base link; the commander SHOULD apply identical rules and treat repeated failures as base-lost (§9.5).

This trailer is uniform across every frame in this spec — decoders may validate integrity before dispatching on ID.

---

## 5. Scaling conventions

| Quantity | Wire type | Scale | Range | Notes |
|---|---|---|---|---|
| linear velocity | `i16` | 1 mm/s | ±32.767 m/s | vx, vy, module speed, odom twist |
| angular velocity | `i16` | 1 mrad/s | ±32.767 rad/s | wz |
| steering angle | `i16` | 1 mrad | ±32.767 rad | module angle |
| pose Δ translation | `i16` | 0.1 mm | ±3.2767 m/frame | dx, dy |
| pose Δ rotation | `i16` | 0.1 mrad | ±3.2767 rad/frame | dθ |
| voltage | `u16` | 10 mV | 0–655.35 V | |
| current | `i16` | 10 mA | ±327.67 A | positive = discharge |
| state of charge | `u8` | 1 % | 0–100 | `0xFF` = unknown |

Encoders MUST saturate to the wire range (no wrap). A **non-finite** (NaN/Inf) source value MUST encode as **zero** — a bad command must never become motion. SI units (m/s, rad/s, rad, m, V, A) are the only units at the API boundary on either side.

---

## 6. Core command frames (commander → base)

### 6.1 `CMD_TWIST` (0x100)

Body-twist setpoint, base frame (x forward, y left, z up).

| Byte | Field | Type | Semantics |
|---|---|---|---|
| 0–1 | `vx` | i16 | linear x, mm/s |
| 2–3 | `vy` | i16 | linear y, mm/s |
| 4–5 | `wz` | i16 | angular z, mrad/s |
| 6 | `counter` | u8 | E2E (§4) |
| 7 | `crc` | u8 | E2E (§4) |

Applied **only in AUTO mode** (§9.2). Components the base cannot realise (per `dof_mask`, §7.4) are ignored; achievable components are clamped to `base.limits.*` — clamping is normal operation, not a fault. A valid, integrity-passing `CMD_TWIST` refreshes the command timeout (§9.1).

### 6.2 `CMD_MODE` (0x101)

| Byte | Field | Type | Semantics |
|---|---|---|---|
| 0 | `mode` | u8 | 0 = standby, 1 = auto |
| 1–5 | reserved | u8[5] | 0 |
| 6 | `counter` | u8 | E2E |
| 7 | `crc` | u8 | E2E |

Requests AUTO or STANDBY. **Necessary but not sufficient** for AUTO — RC override and link freshness gate it (§9.2). Level-latched by the base and **cleared by a command timeout** (§9.1); after a link outage the commander MUST re-send `CMD_MODE(auto)`. Values other than 0/1 are malformed (§10).

### 6.3 `CMD_ESTOP` (0x102)

| Byte | Field | Type | Semantics |
|---|---|---|---|
| 0 | `action` | u8 | 1 = engage, 2 = clear |
| 1–2 | `key` | u16 | clear key; MUST be `0xC1EA` for clear, 0 for engage |
| 3–5 | reserved | u8[3] | 0 |
| 6 | `counter` | u8 | E2E |
| 7 | `crc` | u8 | E2E |

**Asymmetric**: engage is unconditional (any integrity-valid engage latches immediately); clear requires the key `0xC1EA` (hard to trigger accidentally). A clear with a wrong key is malformed and ignored. State machine in §9.6. `CMD_ESTOP` does **not** refresh the command timeout.

### 6.4 `CMD_HEARTBEAT` (0x103)

| Byte | Field | Type | Semantics |
|---|---|---|---|
| 0 | `version` | u8 | protocol version byte (**1**) |
| 1–5 | reserved | u8[5] | 0 |
| 6 | `counter` | u8 | E2E |
| 7 | `crc` | u8 | E2E |

Link liveness. A heartbeat with a **matching version** refreshes the command timeout (§9.1); a mismatched version suspends command processing (§9.4). The commander SHOULD send heartbeats at 10 Hz whenever alive, even when idle, so the base can distinguish "link up, idle" from "link lost".

---

## 7. Core state frames (base → commander)

### 7.1 `STATE_STATUS` (0x200) — base heartbeat

| Byte | Field | Type | Semantics |
|---|---|---|---|
| 0 | `version` | u8 | protocol version byte (**1**) |
| 1 | `mode` | u8 | 0 = standby, 1 = manual, 2 = auto, 3 = estop |
| 2 | `flags` | u8 | bit field, below |
| 3–4 | `faults` | u16 | low 16 bits of the fault word (§10) |
| 5 | reserved | u8 | 0 |
| 6 | `counter` | u8 | E2E |
| 7 | `crc` | u8 | E2E |

This frame is the **base's heartbeat**: the commander MUST treat its absence for longer than a configured base-timeout (default 500 ms, ≥5 nominal periods) as **base-lost** and enter its own safe state (§9.5).

`flags` (bit 0 = LSB): 0 `ESTOP_LATCHED`, 1 `CAN_FRESH` (command timeout satisfied), 2 `RC_LINK_OK` (0 when no RC), 3 `FAILSAFE_ACTIVE` (safe-stop of §9.1), 4 `BUS_DEGRADED` (CAN controller error-passive, §9.7), 5–7 reserved.

### 7.2 `STATE_ODOM_TWIST` (0x201)

Measured body twist (least-squares over the base's wheels/modules).

| Byte | Field | Type | Semantics |
|---|---|---|---|
| 0–1 | `vx` | i16 | mm/s |
| 2–3 | `vy` | i16 | mm/s |
| 4–5 | `wz` | i16 | mrad/s |
| 6 | `counter` | u8 | E2E |
| 7 | `crc` | u8 | E2E |

### 7.3 `STATE_ODOM_POSE` (0x202)

Body-frame displacement accumulated **since the previous frame of this ID** (delta encoding; the consumer integrates on SE(2)). A lost interval is detected via the E2E counter gap (§4) — the interval duration is the configured odom period (advertised as a rate); no on-wire `dt` is needed.

| Byte | Field | Type | Semantics |
|---|---|---|---|
| 0–1 | `dx` | i16 | 0.1 mm, body frame at interval start |
| 2–3 | `dy` | i16 | 0.1 mm |
| 4–5 | `dtheta` | i16 | 0.1 mrad |
| 6 | `counter` | u8 | E2E |
| 7 | `crc` | u8 | E2E |

Small-interval approximation: the delta is integrated in the body frame over one odom period, so intra-interval rotation is not compounded. The consumer SHOULD compose deltas at its own pose estimate.

### 7.4 `STATE_CAPABILITIES` (0x203)

Advertises what the base is and can do — the generic-commander enabler. Sent at 1 Hz and on request (a `CMD_MODE` with an out-of-range value MAY be used as a poll in a future minor version; for now it is periodic).

| Byte | Field | Type | Semantics |
|---|---|---|---|
| 0 | `version` | u8 | protocol version byte (**1**) |
| 1 | `base_type` | u8 | 0 diff, 1 ackermann, 2 omni, 3 mecanum, 4 swerve, 5–254 reserved, 255 custom |
| 2 | `dof_mask` | u8 | bit0 `vx`, bit1 `vy`, bit2 `wz` controllable; others 0 |
| 3 | `wheel_count` | u8 | driven wheels / modules |
| 4 | `model_profile_id` | u8 | 0 = none; registry §8 |
| 5 | `model_profile_ver` | u8 | model profile major version |
| 6 | `counter` | u8 | E2E |
| 7 | `crc` | u8 | E2E |

A commander MUST NOT command a DOF whose `dof_mask` bit is 0 (the base will ignore it); it SHOULD scale its output to `STATE_LIMITS`.

### 7.5 `STATE_LIMITS` (0x204)

| Byte | Field | Type | Semantics |
|---|---|---|---|
| 0–1 | `max_vx` | i16 | mm/s (magnitude) |
| 2–3 | `max_vy` | i16 | mm/s |
| 4–5 | `max_wz` | i16 | mrad/s |
| 6 | `counter` | u8 | E2E |
| 7 | `crc` | u8 | E2E |

The base's configured envelope (`base.limits.*`). Commands beyond these are clamped, not rejected.

### 7.6 `STATE_BATTERY` (0x205)

| Byte | Field | Type | Semantics |
|---|---|---|---|
| 0–1 | `voltage` | u16 | 10 mV |
| 2–3 | `current` | i16 | 10 mA, positive = discharge |
| 4 | `soc` | u8 | %, `0xFF` = unknown |
| 5 | reserved | u8 | 0 |
| 6 | `counter` | u8 | E2E |
| 7 | `crc` | u8 | E2E |

Until a battery sensor is integrated the base sends `voltage = 0, current = 0, soc = 0xFF` (honest "unknown", never invented values).

### 7.7 `STATE_FAULTS` (0x206)

| Byte | Field | Type | Semantics |
|---|---|---|---|
| 0–3 | `faults` | u32 | full fault word (§10) |
| 4 | `last_fault` | u8 | bit index of the most recently raised fault; `0xFF` = none |
| 5 | reserved | u8 | 0 |
| 6 | `counter` | u8 | E2E |
| 7 | `crc` | u8 | E2E |

Sent at 1 Hz and immediately whenever the fault word changes.

---

## 8. Model profiles (extensions)

A model profile is a named, versioned set of extension frames in the ranges of §3, carrying base-type-specific data the Core Profile cannot express. A base advertises exactly one model profile in `STATE_CAPABILITIES.model_profile_id`; a commander loads the matching decoder. Extension frames MUST carry the same E2E trailer (§4) and DLC rule.

**Registry** (`model_profile_id`): 0 none · 1 Swerve Module v1 (§8.1) · 2 Differential Wheels (reserved) · 3 Ackermann Steering (reserved) · 4–254 reserved · 255 vendor-private.

### 8.1 Swerve Module Profile v1 — `STATE_MODULE[i]` (0x220 + i, i ∈ 0..3)

Per-module state; the module index is carried by the frame ID. **Module order** is the kinematics order (`SwerveDriveKinematics::MotorIndex`): 0 front-right, 1 front-left, 2 rear-left, 3 rear-right.

| Byte | Field | Type | Semantics |
|---|---|---|---|
| 0–1 | `speed` | i16 | wheel ground speed, mm/s, signed |
| 2–3 | `angle` | i16 | steering angle, mrad |
| 4 | `fault` | u8 | module fault byte, 0 = ok (device-specific bits) |
| 5 | reserved | u8 | 0 |
| 6 | `counter` | u8 | E2E |
| 7 | `crc` | u8 | E2E |

---

## 9. Safety semantics

### 9.1 Command-timeout failsafe (defined ON the base — non-negotiable)

The base maintains a **command freshness timer**, refreshed only by an integrity-valid `CMD_TWIST` (§6.1) or a version-valid `CMD_HEARTBEAT` (§6.4). Let `T = can.command_timeout_ms` (default **200 ms**). When the base is in AUTO and the timer exceeds `T`:

1. Command a safe stop immediately (zero wheel speeds via the actuator failsafe, `StopAll`), within one control cycle of expiry.
2. Leave AUTO (drop to STANDBY, or to MANUAL if RC override selects it).
3. Clear the latched `CMD_MODE(auto)` request — a restored link does **not** silently resume motion; the commander MUST re-send `CMD_MODE(auto)`.
4. Raise `FAULT_CAN_TIMEOUT` and set `FAILSAFE_ACTIVE` while the condition holds.

Loss of CAN is therefore always a safe stop, enforced by the base itself; no commander cooperation is required or trusted.

### 9.2 Mode arbitration & RC-override precedence

Four modes: STANDBY, MANUAL, AUTO, ESTOP. Arbitrated each control cycle in strict priority:

1. **E-stop** (latched, §9.6) — overrides everything.
2. **RC override**: a configured RC source with a healthy link and its switch in MANUAL forces MANUAL, regardless of any CAN request. The operator always wins.
3. **AUTO** requires **all** of: command freshness (§9.1), a latched `CMD_MODE(auto)`, and — when RC is configured — a healthy RC link with the switch in AUTO.
4. Otherwise **STANDBY**.

The base boots in STANDBY. RC link loss in MANUAL drops to STANDBY (safe stop).

### 9.3 Integrity gate

A command frame failing the E2E checks of §4 (CRC mismatch or counter sequence error) is dropped: it MUST NOT alter control state or refresh freshness, and raises `FAULT_CMD_INTEGRITY`. This is independent of the version gate (§9.4).

### 9.4 Version gate

The link version starts *unknown*. A version-valid heartbeat sets it *valid*; a mismatched heartbeat sets it *invalid*, raises `FAULT_PROTO_VERSION`, and while *invalid* the base treats the link as not fresh (commands not honored, AUTO unreachable) until a version-valid heartbeat arrives. While *unknown* (no heartbeat yet), integrity-valid `CMD_TWIST` freshness alone is accepted, so simple bench tools remain usable; production commanders MUST send heartbeats.

### 9.5 Bidirectional liveness

The base fails safe on commander silence (§9.1). Symmetrically, the **commander** MUST treat loss of `STATE_STATUS` (§7.1) for longer than its configured base-timeout as **base-lost** and enter its own safe state; it SHOULD validate the `STATE_STATUS` counter (§4). Neither side trusts the other to be alive.

### 9.6 E-stop

- **Engage** (`CMD_ESTOP` action 1): latches ESTOP immediately from any state; the base commands `StopAll` every cycle and ignores motion. `FAULT_ESTOP` raised, `ESTOP_LATCHED` set.
- **Clear** (action 2, key `0xC1EA`): unlatches to STANDBY. Motion resumes only through normal arbitration (§9.2) — clearing never directly restarts motion.
- A future RC kill-switch channel latches the same e-stop, cleared the same way.

### 9.7 Bus health

The base monitors its CAN controller error state (ISO 11898-1):

- **Error-passive** (TEC or REC ≥ 128): set `BUS_DEGRADED` in `STATE_STATUS`; continue operating (the bus may still be usable).
- **Bus-off** (TEC ≥ 256): command a safe stop immediately, raise `FAULT_CAN_BUSOFF`, and **auto-recover into STANDBY** — after the controller re-joins the bus, the base MUST require the normal AUTO re-arm (§9.2); it MUST NOT silently resume motion.

---

## 10. Fault word

Sticky since boot (raised, never cleared except by restart) — it answers "has this ever gone wrong on this run". Live conditions are in `STATE_STATUS.flags`. Bits 0–15 mirror into `STATE_STATUS.faults`.

| Bit | Name | Raised when |
|---|---|---|
| 0 | `FAULT_CAN_TIMEOUT` | the §9.1 failsafe tripped |
| 1 | `FAULT_PROTO_VERSION` | a heartbeat with an unknown version byte was received |
| 2 | `FAULT_MALFORMED_FRAME` | a defined ID arrived with bad DLC, an undecodable field, or a wrong e-stop key |
| 3 | `FAULT_RC_LINK_LOST` | a configured RC link was lost |
| 4 | `FAULT_ACTUATOR_COMMAND` | an actuator command returned non-OK |
| 5 | `FAULT_ACTUATOR_READ` | a module-state read returned non-OK |
| 6 | `FAULT_ESTOP` | the e-stop was engaged |
| 7 | `FAULT_CAN_BUSOFF` | the CAN controller entered bus-off (§9.7) |
| 8 | `FAULT_CMD_INTEGRITY` | a command frame failed the E2E CRC or counter check (§4) |
| 9–31 | reserved | 0 |

---

## 11. Bus-load budget

At nominal rates and 500 kbit/s (worst-case ~130 bit/frame incl. stuffing):

| Frame | Rate (Hz) | frames/s |
|---|---|---|
| CMD_TWIST | 50 | 50 |
| CMD_HEARTBEAT | 10 | 10 |
| STATE_STATUS | 10 | 10 |
| STATE_ODOM_TWIST | 50 | 50 |
| STATE_ODOM_POSE | 50 | 50 |
| STATE_MODULE×4 | 20 | 80 |
| STATE_CAPABILITIES/LIMITS/BATTERY/FAULTS | 1 each | 4 |
| **Total** | | **≈ 254** |

≈ 254 × 130 = **33 kbit/s ≈ 6.6 %** at 500 kbit/s. Command/mode/e-stop bursts are event-driven and negligible. Ample headroom for model extensions; a base adding heavy extension traffic SHOULD re-check this budget.

---

## 12. Conformance checklist (base implementation)

- [ ] Every frame encodes/decodes with a valid E2E trailer; decoders reject wrong ID, wrong DLC, and CRC mismatch (`test_can_codec.cpp` round-trips every frame + integrity cases).
- [ ] Alive-counter validation: in-order accept, stuck/replay reject, gap ≤ MaxDelta tolerated, gap > MaxDelta rejected.
- [ ] Timeout-failsafe per §9.1 incl. auto-request clear (`test_failsafe.cpp`).
- [ ] Integrity gate §9.3, version gate §9.4 (`test_failsafe.cpp`).
- [ ] Mode arbitration & RC precedence §9.2 (`test_fsm.cpp`).
- [ ] E-stop latch / clear-key §9.6 (`test_fsm.cpp`, `test_can_codec.cpp`).
- [ ] Bus-off → safe stop + STANDBY re-arm §9.7.
- [ ] `.dbc` matches the codec (scales, IDs, layout) — CI check §13.
- [ ] On-bus timing of state rates measured on hardware (bench bring-up).

---

## 13. Machine-readable dictionary (DBC)

`docs/mobile_base.dbc` is the tool-consumable message dictionary (Vector DBC) covering every frame here — for CANalyzer/SavvyCAN/BusMaster decode and codegen. It is the same source of truth as this document; a CI test asserts the codec's IDs and scales match the DBC. (The DBC cannot express the E2E counter/CRC semantics of §4 or the safety rules of §9 — those remain normative here.)

---

## Appendix A. Multi-base addressing (optional, informative)

For multiple bases on one bus, use **29-bit extended IDs**: `ext_id = (source_addr << 18) | (0 << 17) | std_id`, where `source_addr` is an 8-bit per-base address and `std_id` is the 11-bit ID from §3. The commander addresses a base by its `source_addr`; a base ignores frames whose `source_addr` is not its own. The core 11-bit mapping is the single-base default; this appendix is not part of v1.0.0 conformance.

## Open items

| Item | Section | Resolves |
|---|---|---|
| Battery sensing (real values) | §7.6 | hardware bring-up |
| RC kill-switch channel onto the e-stop latch | §9.6 | bench decision |
| On-request capabilities poll opcode | §7.4 | minor version |
| Differential / Ackermann model profiles | §8 | as bases are added |

# AckermannVescBase — design note (planned)

A **host-composed** `MobileBase`: an Ackermann RC car whose steering servo **and**
drive motor are both driven by a **VESC over CAN**, directly from this computer —
no firmware base, no wire protocol. It realizes the abstract
[`xmotion::MobileBase`](../mobile_base/include/mobile_base/mobile_base.hpp)
in-process, so navigation drives it with the exact same `SetTwist` / `Read*`
calls it uses for the swervebot `MobileBaseCanClient`. Adding this robot is a new
class behind the existing interface — **zero navigation changes**.

This is the `robots/` **Pattern B** (compose `devices/` + kinematics), the
counterpart to `MobileBaseCanClient`'s Pattern A (serialize a protocol over a
wire). It's the concrete answer to "robots can use the device abstractions."

```
   navigation ──SetTwist(vx,vy,wz)──▶ MobileBase (abstract)
                                          ▲
                                          │ implements
                                 AckermannVescBase
                     twist ─▶ bicycle model ─▶ (δ steer, v drive)
                                          │
                              xmotion::VescMotor  (one VESC, CAN)
                                 ├─ SetServo(δ→[0,1])   steering
                                 └─ SetSpeed(v→ERPM)    drive
                                          │
                              VESC feedback (ERPM, tach, V, I, temp, fault)
                                          ▼
                                 wheel odometry ─▶ ReadOdomTwist / ReadOdomPose
```

## Composition

One `xmotion::VescMotor` (from `motor_vesc`). The VESC already exposes both
outputs on CAN — `SetSpeed`/`SetCurrent` for the drive motor and `SetServo` for
the steering servo on its servo output — so the whole base is one device. No
separate servo driver needed. Depends on `mobile_base` (the contract +
Core Profile types) and `motor_vesc`.

## Kinematics (rear-axle bicycle model), wheelbase `L`

**Command → actuation** (`SetTwist(vx, vy, wz)`; `vy` unrealizable, ignored):
- steering  `δ = atan(wz · L / vx)`, clamped to `±δ_max`
- drive     `v = vx`, clamped to `±v_max`
- `SetServo(center + δ / δ_max · half_range)` · `SetSpeed(v · k_erpm)`
  where `k_erpm = pole_pairs · gear_ratio · 60 / wheel_circumference`.
- **Low speed:** as `vx → 0`, `δ` is indeterminate and the car cannot rotate in
  place (nonholonomic). Below a threshold, hold steering and command `v = 0`
  rather than fabricate a turn. This constraint is advertised, not hidden (below).

**Feedback → odometry** (from the VESC state callback):
- `v_meas = ERPM_meas / k_erpm`  (tachometer gives distance for drift-free integration)
- steering is **open-loop** — no shaft sensor — so use the commanded `δ` as the
  estimate (a steering-angle sensor would upgrade this; note it as a known limit).
- `wz = v_meas · tan(δ) / L`; integrate `(v_meas, wz)` → `dx, dy, dtheta`.
- publish `ReadOdomTwist{vx=v_meas, vy=0, wz}` and the accumulated `ReadOdomPose`.

Reuse xmNavigation's bicycle/Ackermann kinematics if present; don't duplicate the
model.

## Capabilities & limits (the adaptation seam)

- `ReadCapabilities` → `base_type = kAckermann`, `dof_mask = kDofVx | kDofWz`
  (no `vy`), `wheel_count`, `model_profile_id` (an Ackermann model profile if
  per-wheel detail is ever needed).
- `ReadLimits` → `max_vx = v_max`, `max_wz = v_max · tan(δ_max) / L` (curvature-
  limited), `max_vy = 0`.

A planner reads these and constrains its motion model by **parameter** — the same
descriptor the swerve base fills differently. That's what lets one framework drive
both without branching on robot identity.

## Status, battery, faults

Synthesize the Core Profile state from the VESC: `ReadBattery` from input
voltage/current; `ReadFaults` from the VESC fault code (+ over-temp); `ReadStatus`
mode/flags from the base's own control state. `Health()` degrades on VESC fault or
stale feedback via the same `FreshnessMonitor` the CAN client uses.

## Safety / failsafe

No wire to go stale, so **liveness is the host control loop's cadence**: if the
owner stops calling `SetTwist` within a timeout, command `v = 0`. `EngageEStop` →
VESC brake (or zero current) + center steering; `ClearEStop` re-arms. E-stop and
the low-speed rule are enforced in this class, not left to the caller.

## Config

`wheelbase_m`, `wheel_radius_m`, `pole_pairs`, `gear_ratio`, `v_max`,
`steer_max_rad`, servo calibration (`center`, `half_range`, invert),
`command_timeout`, VESC CAN id + interface.

## Module layout

```
src/robots/ackermann_vesc/
  include/ackermann_vesc/ackermann_vesc_base.hpp   class AckermannVescBase : public MobileBase
  src/ackermann_vesc_base.cpp
  test/test_ackermann_vesc_base.cpp                fake VescMotor: assert servo+speed vs bicycle
                                                   model; feed ERPM → assert odometry integration
  CMakeLists.txt                                   deps: mobile_base, motor_vesc
```

## Testing (hardware-free)

Inject a fake VESC (the `VescCanInterface` accepts an injected `CanInterface`, or
stub `VescMotor`): drive `SetTwist` and assert the servo position + ERPM match the
bicycle model at several `(vx, wz)` including the low-speed hold; push synthetic
ERPM/tachometer feedback and assert `ReadOdomTwist`/`ReadOdomPose` integrate
correctly; assert `ReadCapabilities` reports Ackermann DOF and `EngageEStop` zeros
drive + centers steering.

## Open questions

- **Open-loop steering** — commanded δ ≠ actual under load. Acceptable for v1;
  a steering feedback sensor (or current-based estimate) is the upgrade path.
- **VESC speed vs current mode** — closed-loop ERPM (`SetSpeed`) is simplest for
  odometry; `SetCurrent` gives better low-speed control but needs a speed loop.
  Start with `SetSpeed`.
- Does xmNavigation already provide the bicycle model + odometry integrator? If
  so, this class is mostly glue over it.

# ADR 0001 — HAL interface redesign for hardware-independence

- Status: Proposed (reference stage landed; full migration pending)
- Date: 2026-06-30
- Scope: `xmmu/hal` — the device interface layer consumed by xmNabla and per-robot apps

## Context

The goal is a HAL where **swapping a hardware implementation does not require upper-layer code to change**. The current `hal/` has interfaces but leaks implementation detail on four axes, so a VESC↔AKELC↔Waveshare swap still edits app code:

1. **Inconsistent lifecycle.** `Open/Close/IsOpened` (CAN, serial, RC) vs `Connect/Disconnect/IsConnected` (IMU) vs `Connect/Disconnect/IsOkay` (Sensor); `MotorControllerInterface` has no lifecycle at all.
2. **Transport leaks into the contract.** `Connect(dev_name, baud_rate)` bakes in "serial with a baud rate"; `SensorInterface` throws for the "wrong" transport. `CanInterface` exposes `struct can_frame` (`linux/can.h`) to consumers.
3. **Capability by exception.** Unsupported operations `throw std::runtime_error`, so capability is discovered at runtime by catching, and a speed↔position swap changes behavior from working to throwing.
4. **Four error conventions.** `bool` / `void` / `throw` / `0`-sentinel coexist; a getter returning `0` on a bus error makes a dead device look healthy.

Plus: no units in the type system (bare `float`), ad-hoc health (`IsNormal`/`IsOkay`/`frame_loss`), and no construction seam (apps `#include` and `new` the concrete driver).

## Decision

Adopt a layered, capability-segregated, transport-agnostic, error-uniform HAL with a construction seam. Decisions taken (with the rejected alternatives):

| Axis | Decision | Rejected |
|------|----------|----------|
| Error model | `Status` for commands, `Result<T>` for reads (C++17, no exceptions on routine paths) | exceptions (RT-unsafe); `std::expected` (C++23) |
| Capabilities | Segregated mixin interfaces (`SpeedControllable`, …) + mandatory `SafetyStoppable` | one fat interface + `Capabilities()` query; status-quo throwing |
| Namespace | New `xmotion::hal` namespace, separate from legacy `xmotion` interfaces | reuse `xmotion` (clashes with legacy types during migration) |
| Construction | `MotorFactory` registry keyed by a config `type` string; drivers self-register | direct construction in app code |
| Lifecycle | Uniform `Device` base: `Connect()/Disconnect()/IsConnected()/Health()`; transport params at construction | per-interface lifecycle vocabularies |
| Units | Strong scalar types (`Rpm`, `Radian`, `Ampere`, `NewtonMetre`, `Ratio`) | bare `float` |
| Staging | ADR + reference interface + reference impl first, then incremental driver migration | clean break across 3 repos in one change |

### Layering

- `xmotion::hal` (this layer, app-facing, stable): `Device`, capability mixins, `Motor`, `MotorFactory`, unit types, `Status`/`Result`. **Apps depend only on this.**
- `xmmu/transport/` (below the HAL): the transport interfaces (`serial_interface.hpp`, `can_interface.hpp`, `modbus_rtu_interface.hpp`, `modbus_rtu_client.hpp`) — **relocated here** out of the app-facing `xmmu/hal/` so device consumers don't see transport types like `can_frame`. Compatibility facades remain at the old `xmmu/hal/*` paths. (Namespace stays `xmotion` for now; a dedicated `xmotion::transport` namespace is a later, higher-churn step.)

### Capability model

```
Motor = Device + SafetyStoppable          // every actuator: connectable, stoppable
      [+ SpeedControllable]               // as supported by the hardware
      [+ PositionControllable]
      [+ TorqueControllable]
      [+ CurrentControllable]
      [+ Brakable]
```

A driver inherits only the mixins it implements. An upper layer depends on the mixin it needs and discovers extras with `dynamic_cast` (mixins are polymorphic). `SafetyStoppable::Stop()` is the failsafe hook the old interface lacked — every actuator must define a safe state.

Commands clamp to the configured safe envelope and report `kOutOfRange` (clamp **and** report — never silent saturation), and reject non-finite input with `kInvalidArgument`. Bounded motion is part of the contract, not the caller's responsibility.

### Construction seam

```cpp
MotorConfig cfg{ .type = "vesc", .bus = "can0", .id = 3, .max_speed_rpm = 6000 };
std::unique_ptr<hal::Motor> motor = hal::MotorFactory::Instance().Create(cfg);
```

Hardware is selected by the config `type` string; the app includes no driver header and names no concrete driver. Swapping `"vesc"` → `"akelc"` is a config edit.

## Worked example — how VESC maps onto the new HAL

```cpp
// xmmu::transport::CanBus is the (future) internal transport; the driver wraps it.
class VescMotor final : public hal::Motor,
                        public hal::SpeedControllable,
                        public hal::CurrentControllable {
 public:
  VescMotor(std::string bus, int id, double max_rpm, double max_a);

  hal::Status Connect() override;          // open the CAN bus, start RX
  void        Disconnect() override;       // Stop() first, then close
  bool        IsConnected() const override;
  hal::DeviceHealth Health() const override;  // map VESC fault flags -> state

  hal::Status Stop() override {            // failsafe: zero current
    return SetCurrent(hal::Ampere{0.0});
  }
  hal::Status SetSpeed(hal::Rpm s) override;   // clamp to max_rpm, NaN-check, send
  hal::Result<hal::Rpm> GetSpeed() override;   // kNotConnected/kTimeout vs real value
  hal::Status SetCurrent(hal::Ampere a) override;
  hal::Result<hal::Ampere> GetCurrent() override;
};
// self-register:  static const hal::MotorRegistrar kReg{"vesc", ...};
```

The legacy `vesc_can_interface` bugs found in the code review (unclamped RPM, `0`-on-error reads, no failsafe on disconnect) are *resolved by construction* once the driver satisfies this contract.

## Consequence for upper layers (xmNabla)

`SpeedActuatorGroup` today takes `vector<shared_ptr<MotorControllerInterface>>` and forwards `SetSpeed(float)`. Under the new HAL it composes `SpeedControllable` (and is itself a `Motor` + `SpeedControllable`), gains a real `Stop()` that fans out to every member, and propagates `Status` instead of swallowing failures. Apps build the group from config and never name a driver.

## Migration plan (incremental, CI-verified)

1. **Reference stage:** land the `xmotion::hal` interface layer + the `SimMotorController` reference impl + tests, additive and non-breaking. ✅
2. Migrate the VESC driver to `VescMotor` behind the factory (`"vesc"`); keep `vesc_can_interface` until consumers move. The unclamped-RPM, `0`-on-error, and no-failsafe-on-disconnect defects from the code review are fixed by construction in `VescMotor`. ✅
3. Provide a `LegacyMotorAdapter` (`MotorControllerInterface` ⇄ `hal::Motor`) so xmNabla compiles unchanged while it migrates.
4. Migrate AKELC, Waveshare, then the sensor/RC/HID families on the same template.
5. Move `serial`/`can`/`modbus` interfaces into `xmmu/transport/`, out of the app-facing HAL. ✅ (dir-move + facades; `xmotion::transport` namespace deferred)
6. Update xmNabla's actuator groups; verify the full Σ+μ+∇ build via the umbrella Assembly CI; drop the legacy interfaces.

Each step is verified by the per-component CI and the umbrella Assembly job (which builds Σ+μ+∇ together).

## Status of the reference stage

Landed header-only under `xmmu/hal/`: `status.hpp`, `result.hpp`, `units.hpp`, `device.hpp`, `motor_controller.hpp`, `motor_factory.hpp`, and `reference/sim_motor_controller.hpp`, with `test/test_hal_interfaces.cpp`. The new layer is additive — no existing interface or driver is changed — so xmNabla and the apps are unaffected until step 2+.

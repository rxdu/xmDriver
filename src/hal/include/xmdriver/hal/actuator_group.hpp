/*
 * actuator_group.hpp
 *
 * Actuator groups — pure HAL composition (moved here from xmNavigation's
 * robot_base per ADR 0005: algorithm components depend on xmBase only, hardware
 * composition lives in the driver layer). If several motors always receive the
 * same command (e.g. the two wheels on one side of a skid-steer base), a group
 * lets the caller treat them as a single actuator.
 *
 * Semantics preserved from the old robot_base groups:
 *   - fan-out strictly in member registration order,
 *   - every member is commanded on every group command, even after an earlier
 *     member fails (a half-commanded base is worse than a fully-commanded one
 *     with one degraded wheel),
 *   - reads (GetSpeed/GetPosition) return the arithmetic mean of the members.
 * What changed: commands now return an aggregated Status (first failing member
 * status; the old interface returned void and dropped errors), reads return
 * Result instead of averaging error zeros in, and every group carries the
 * mandatory Stop() failsafe.
 *
 * Members are typed as the capability actually commanded (SpeedControllable /
 * PositionControllable) plus the mandatory SafetyStoppable failsafe. Concrete
 * drivers implement both mixins on one object, so one motor registers once:
 *
 *   SimMotorController left_a, left_b;
 *   SpeedActuatorGroup left({{left_a}, {left_b}});
 *   left.SetSpeed(Rpm{120});   // broadcast, in registration order
 *   left.Stop();               // failsafe fan-out
 *
 * Groups are observability-instrumented but never alter control flow: a
 * pre-registered fault counter (driver.actuator_group.command_fault_count)
 * counts every failed member command, and one XM_WARN_SRC fires per failure
 * episode (first failing group command after a fully-successful one), not per
 * call — the command hot path stays allocation-free and span-free.
 *
 * Groups do not own their members (the composing application does) and are not
 * thread-safe, matching the old groups; serialize access externally.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

#include "xmbase/telemetry/telemetry.hpp"
#include "xmdriver/hal/motor_controller.hpp"

namespace xmotion::hal {

namespace detail {

// Shared instrumentation + aggregation state for the actuator groups: the
// pre-registered fault counter, the group event source, and the one-warn-per-
// failure-episode discipline. Header-only and free of hot-path allocation:
// counting is one atomic add, the warn fires only on the episode edge.
class ActuatorGroupInstrumentation {
 protected:
  ActuatorGroupInstrumentation() = default;

  // Fold one member command status into the aggregate (first failure wins) and
  // count the fault. Never short-circuits — callers keep fanning out.
  void NoteMemberStatus(Status member_status, Status& aggregate) {
    if (member_status == Status::kOk) return;
    fault_metric_.Add();
    if (aggregate == Status::kOk) aggregate = member_status;
  }

  // Close out one group command: warn once on the no-fault -> fault edge
  // (`what` must be a string literal), re-arm when a command is clean again.
  void FinishCommand(const char* what, Status aggregate) {
    if (aggregate == Status::kOk) {
      in_fault_episode_ = false;
      return;
    }
    if (!in_fault_episode_) {
      in_fault_episode_ = true;
      XM_WARN_SRC(src_, "actuator group: {} failed on >=1 member: {}", what,
                  ToString(aggregate));
    }
  }

 private:
  telemetry::EventSource src_ =
      telemetry::GetEventSource("driver.actuator_group");
  telemetry::Counter fault_metric_ =
      telemetry::GetCounter("driver.actuator_group.command_fault_count");
  bool in_fault_episode_ = false;
};

}  // namespace detail

// One group member: the command capability plus the mandatory failsafe hook.
// Both usually come from the same driver object (motors implement the mixins
// they support), so the single-argument form is the common spelling.
struct SpeedGroupMember {
  template <typename T,
            typename = std::enable_if_t<
                std::is_base_of<SpeedControllable, T>::value &&
                std::is_base_of<SafetyStoppable, T>::value>>
  SpeedGroupMember(T& motor) : speed(motor), stop(motor) {}  // NOLINT(runtime/explicit)
  SpeedGroupMember(SpeedControllable& s, SafetyStoppable& st)
      : speed(s), stop(st) {}

  SpeedControllable& speed;
  SafetyStoppable& stop;
};

class SpeedActuatorGroup final : private detail::ActuatorGroupInstrumentation {
 public:
  explicit SpeedActuatorGroup(std::vector<SpeedGroupMember> members)
      : members_(std::move(members)) {}

  std::size_t size() const noexcept { return members_.size(); }

  // Broadcast the same speed to every member, in registration order. Every
  // member is commanded even if an earlier one fails; returns the first
  // failing member's status (kOk when all succeeded).
  Status SetSpeed(Rpm speed) {
    Status aggregate = Status::kOk;
    for (SpeedGroupMember& m : members_) {
      NoteMemberStatus(m.speed.SetSpeed(speed), aggregate);
    }
    FinishCommand("SetSpeed", aggregate);
    return aggregate;
  }

  // Per-member fan-out: speeds[i] goes to member i, in registration order.
  // `count` must equal size(); on mismatch nothing is commanded (a partially
  // applied command vector is unsafe) and kInvalidArgument is returned.
  Status SetSpeed(const Rpm* speeds, std::size_t count) {
    if (speeds == nullptr || count != members_.size()) {
      return Status::kInvalidArgument;
    }
    Status aggregate = Status::kOk;
    for (std::size_t i = 0; i < count; ++i) {
      NoteMemberStatus(members_[i].speed.SetSpeed(speeds[i]), aggregate);
    }
    FinishCommand("SetSpeed", aggregate);
    return aggregate;
  }

  Status SetSpeed(const std::vector<Rpm>& speeds) {
    return SetSpeed(speeds.data(), speeds.size());
  }

  // Arithmetic mean of the member speeds (the old group's read semantics).
  // Every member is read even if an earlier read fails; a failed member read
  // fails the whole group read (first failing status) instead of silently
  // averaging error zeros in. Empty groups have no mean: kInvalidArgument.
  Result<Rpm> GetSpeed() {
    if (members_.empty()) return Result<Rpm>::Err(Status::kInvalidArgument);
    Status aggregate = Status::kOk;
    double sum = 0.0;
    for (SpeedGroupMember& m : members_) {
      const Result<Rpm> r = m.speed.GetSpeed();
      if (r.ok()) {
        sum += r.value.value();
      } else if (aggregate == Status::kOk) {
        aggregate = r.status;
      }
    }
    if (aggregate != Status::kOk) return Result<Rpm>::Err(aggregate);
    return Result<Rpm>::Ok(Rpm{sum / static_cast<double>(members_.size())});
  }

  // Failsafe fan-out: Stop() every member unconditionally, in registration
  // order, regardless of individual failures; returns the first failure.
  Status Stop() {
    Status aggregate = Status::kOk;
    for (SpeedGroupMember& m : members_) {
      NoteMemberStatus(m.stop.Stop(), aggregate);
    }
    FinishCommand("Stop", aggregate);
    return aggregate;
  }

 private:
  std::vector<SpeedGroupMember> members_;
};

// Same shape over PositionControllable (Radian commands).
struct PositionGroupMember {
  template <typename T,
            typename = std::enable_if_t<
                std::is_base_of<PositionControllable, T>::value &&
                std::is_base_of<SafetyStoppable, T>::value>>
  PositionGroupMember(T& motor) : position(motor), stop(motor) {}  // NOLINT(runtime/explicit)
  PositionGroupMember(PositionControllable& p, SafetyStoppable& st)
      : position(p), stop(st) {}

  PositionControllable& position;
  SafetyStoppable& stop;
};

class PositionActuatorGroup final
    : private detail::ActuatorGroupInstrumentation {
 public:
  explicit PositionActuatorGroup(std::vector<PositionGroupMember> members)
      : members_(std::move(members)) {}

  std::size_t size() const noexcept { return members_.size(); }

  // Broadcast the same position to every member, in registration order; every
  // member is commanded even after a failure, first failing status returned.
  Status SetPosition(Radian position) {
    Status aggregate = Status::kOk;
    for (PositionGroupMember& m : members_) {
      NoteMemberStatus(m.position.SetPosition(position), aggregate);
    }
    FinishCommand("SetPosition", aggregate);
    return aggregate;
  }

  // Per-member fan-out: positions[i] goes to member i. `count` must equal
  // size(); on mismatch nothing is commanded and kInvalidArgument is returned.
  Status SetPosition(const Radian* positions, std::size_t count) {
    if (positions == nullptr || count != members_.size()) {
      return Status::kInvalidArgument;
    }
    Status aggregate = Status::kOk;
    for (std::size_t i = 0; i < count; ++i) {
      NoteMemberStatus(members_[i].position.SetPosition(positions[i]),
                       aggregate);
    }
    FinishCommand("SetPosition", aggregate);
    return aggregate;
  }

  Status SetPosition(const std::vector<Radian>& positions) {
    return SetPosition(positions.data(), positions.size());
  }

  // Arithmetic mean of the member positions; same read semantics as
  // SpeedActuatorGroup::GetSpeed().
  Result<Radian> GetPosition() {
    if (members_.empty()) return Result<Radian>::Err(Status::kInvalidArgument);
    Status aggregate = Status::kOk;
    double sum = 0.0;
    for (PositionGroupMember& m : members_) {
      const Result<Radian> r = m.position.GetPosition();
      if (r.ok()) {
        sum += r.value.value();
      } else if (aggregate == Status::kOk) {
        aggregate = r.status;
      }
    }
    if (aggregate != Status::kOk) return Result<Radian>::Err(aggregate);
    return Result<Radian>::Ok(
        Radian{sum / static_cast<double>(members_.size())});
  }

  // Failsafe fan-out over every member, in registration order.
  Status Stop() {
    Status aggregate = Status::kOk;
    for (PositionGroupMember& m : members_) {
      NoteMemberStatus(m.stop.Stop(), aggregate);
    }
    FinishCommand("Stop", aggregate);
    return aggregate;
  }

 private:
  std::vector<PositionGroupMember> members_;
};

}  // namespace xmotion::hal

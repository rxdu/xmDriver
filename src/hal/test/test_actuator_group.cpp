/*
 * test_actuator_group.cpp
 *
 * Behavioral contract of the HAL actuator groups (moved from xmNavigation's
 * robot_base per ADR 0005), exercised through scripted capability fakes (order
 * and failure injection) and the reference SimMotorController (real clamp /
 * unit semantics):
 *   - fan-out strictly in member registration order,
 *   - unit-typed commands pass through unmodified,
 *   - a member failure never stops the fan-out; the group aggregates the
 *     first failing status,
 *   - Stop() is an unconditional failsafe fan-out,
 *   - the pre-registered fault counter counts every failed member command and
 *     XM_WARN_SRC fires once per failure episode (observed via the capture
 *     binding, the pattern from test_health_telemetry.cpp).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

#include "xmdriver/hal/actuator_group.hpp"
#include "xmdriver/hal/reference/sim_motor_controller.hpp"

namespace tel = xmotion::telemetry;
using namespace xmotion::hal;

namespace {

// ---- capture binding (test_health_telemetry.cpp pattern) -------------------
// Counters are name-dispatched so the group's pre-registered fault counter is
// individually observable; warn-severity events are counted to verify the
// once-per-episode discipline.

std::mutex g_mtx;
int g_warn_events = 0;

tel::detail::CounterSlot& FaultCounterSlot() {
  static tel::detail::CounterSlot slot;
  return slot;
}

int TakeWarnCount() {
  std::lock_guard<std::mutex> lk(g_mtx);
  return g_warn_events;
}

const tel::Binding* CaptureBinding() {
  static tel::detail::CounterSlot other_counter;
  static tel::detail::GaugeSlot gauge;
  static tel::detail::HistogramSlot histogram;
  static tel::detail::SignalSlot signal;
  static tel::Binding b = [] {
    tel::Binding x{};
    x.abi_version = tel::kBindingAbiVersion;
    x.get_counter = [](std::string_view name) {
      return name == "driver.actuator_group.command_fault_count"
                 ? &FaultCounterSlot()
                 : &other_counter;
    };
    x.get_gauge = [](std::string_view) { return &gauge; };
    x.get_histogram = [](std::string_view) { return &histogram; };
    x.get_signal = [](std::string_view, std::size_t, std::size_t,
                      const char*) { return &signal; };
    x.intern_source = [](std::string_view) { return 1u; };
    x.should_log = [](tel::Severity) noexcept { return true; };
    x.set_level = [](tel::Severity) noexcept {};
    x.get_level = []() noexcept { return tel::Severity::kTrace; };
    x.emit_event = [](std::uint32_t, tel::Severity sev, const char*,
                      const tel::detail::ArgPack&, tel::Context,
                      tel::Timestamp) noexcept {
      if (sev == tel::Severity::kWarn) {
        std::lock_guard<std::mutex> lk(g_mtx);
        ++g_warn_events;
      }
    };
    x.emit_event_dyn = [](std::uint32_t, tel::Severity, const char*,
                          std::size_t, tel::Context,
                          tel::Timestamp) noexcept {};
    x.emit_span = [](const char*, tel::Context, tel::SpanId, tel::Timestamp,
                     tel::Timestamp, const tel::Context*,
                     std::uint8_t) noexcept {};
    x.emit_signal = [](tel::detail::SignalSlot*, const void*, std::size_t,
                       tel::Timestamp) noexcept {};
    x.report_health = [](const char*, tel::HealthState, const char*,
                         tel::Context, tel::Timestamp) noexcept {};
    x.set_resource = [](std::string_view, std::string_view) {};
    return x;
  }();
  return &b;
}

// ---- scripted capability fake ----------------------------------------------
// Records every call (id + op + commanded value) into a shared log so member
// ordering across the group is observable, and returns scripted statuses.

struct Call {
  int id;
  std::string op;
  double value;
};

class ScriptedMotor final : public SpeedControllable,
                            public PositionControllable,
                            public SafetyStoppable {
 public:
  ScriptedMotor(int id, std::vector<Call>* log) : id_(id), log_(log) {}

  Status SetSpeed(Rpm speed) override {
    log_->push_back({id_, "SetSpeed", speed.value()});
    return set_speed_status;
  }
  Result<Rpm> GetSpeed() override {
    log_->push_back({id_, "GetSpeed", 0.0});
    if (get_speed_status != Status::kOk) return Result<Rpm>::Err(get_speed_status);
    return Result<Rpm>::Ok(Rpm{speed_reading});
  }

  Status SetPosition(Radian position) override {
    log_->push_back({id_, "SetPosition", position.value()});
    return set_position_status;
  }
  Result<Radian> GetPosition() override {
    log_->push_back({id_, "GetPosition", 0.0});
    if (get_position_status != Status::kOk)
      return Result<Radian>::Err(get_position_status);
    return Result<Radian>::Ok(Radian{position_reading});
  }

  Status Stop() override {
    log_->push_back({id_, "Stop", 0.0});
    return stop_status;
  }

  Status set_speed_status = Status::kOk;
  Status get_speed_status = Status::kOk;
  Status set_position_status = Status::kOk;
  Status get_position_status = Status::kOk;
  Status stop_status = Status::kOk;
  double speed_reading = 0.0;
  double position_reading = 0.0;

 private:
  int id_;
  std::vector<Call>* log_;
};

std::vector<int> IdsFor(const std::vector<Call>& log, const std::string& op) {
  std::vector<int> ids;
  for (const Call& c : log)
    if (c.op == op) ids.push_back(c.id);
  return ids;
}

class ActuatorGroupTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Install BEFORE constructing any group: the fault counter and event
    // source are pre-registered in the group's constructor.
    ASSERT_TRUE(tel::InstallBinding(CaptureBinding()));
    FaultCounterSlot().value.store(0.0);
    std::lock_guard<std::mutex> lk(g_mtx);
    g_warn_events = 0;
  }

  double FaultCount() const { return FaultCounterSlot().value.load(); }
};

// ---- fan-out order & unit passthrough ---------------------------------------

TEST_F(ActuatorGroupTest, SpeedBroadcastFansOutInRegistrationOrder) {
  std::vector<Call> log;
  ScriptedMotor m1(1, &log), m2(2, &log), m3(3, &log);
  SpeedActuatorGroup group({{m1}, {m2}, {m3}});
  ASSERT_EQ(group.size(), 3u);

  EXPECT_EQ(group.SetSpeed(Rpm{120.0}), Status::kOk);
  EXPECT_EQ(IdsFor(log, "SetSpeed"), (std::vector<int>{1, 2, 3}));
  for (const Call& c : log) EXPECT_DOUBLE_EQ(c.value, 120.0);  // passthrough
}

TEST_F(ActuatorGroupTest, SpeedPerMemberFanOutMapsByIndexInOrder) {
  std::vector<Call> log;
  ScriptedMotor m1(1, &log), m2(2, &log), m3(3, &log);
  SpeedActuatorGroup group({{m1}, {m2}, {m3}});

  const std::vector<Rpm> cmds{Rpm{10.0}, Rpm{20.0}, Rpm{30.0}};
  EXPECT_EQ(group.SetSpeed(cmds), Status::kOk);
  ASSERT_EQ(log.size(), 3u);
  EXPECT_EQ(IdsFor(log, "SetSpeed"), (std::vector<int>{1, 2, 3}));
  EXPECT_DOUBLE_EQ(log[0].value, 10.0);
  EXPECT_DOUBLE_EQ(log[1].value, 20.0);
  EXPECT_DOUBLE_EQ(log[2].value, 30.0);
}

TEST_F(ActuatorGroupTest, SpeedPerMemberSizeMismatchCommandsNothing) {
  std::vector<Call> log;
  ScriptedMotor m1(1, &log), m2(2, &log);
  SpeedActuatorGroup group({{m1}, {m2}});

  const std::vector<Rpm> too_short{Rpm{10.0}};
  EXPECT_EQ(group.SetSpeed(too_short), Status::kInvalidArgument);
  EXPECT_EQ(group.SetSpeed(nullptr, 2), Status::kInvalidArgument);
  EXPECT_TRUE(log.empty()) << "mismatched command vector must not be applied";
  EXPECT_DOUBLE_EQ(FaultCount(), 0.0) << "caller error is not a member fault";
}

TEST_F(ActuatorGroupTest, UnitPassthroughWithSimMotors) {
  SimMotorController a, b;
  ASSERT_EQ(a.Connect(), Status::kOk);
  ASSERT_EQ(b.Connect(), Status::kOk);
  SpeedActuatorGroup group({{a}, {b}});

  EXPECT_EQ(group.SetSpeed(Rpm{123.5}), Status::kOk);
  EXPECT_DOUBLE_EQ(a.GetSpeed().value.value(), 123.5);
  EXPECT_DOUBLE_EQ(b.GetSpeed().value.value(), 123.5);

  const Result<Rpm> avg = group.GetSpeed();
  ASSERT_TRUE(avg.ok());
  EXPECT_DOUBLE_EQ(avg.value.value(), 123.5);
}

// ---- partial failure: aggregation + continued fan-out ----------------------

TEST_F(ActuatorGroupTest, PartialFailureAggregatesFirstFailureAndContinues) {
  std::vector<Call> log;
  ScriptedMotor m1(1, &log), m2(2, &log), m3(3, &log);
  m2.set_speed_status = Status::kTimeout;
  m3.set_speed_status = Status::kDeviceFault;
  SpeedActuatorGroup group({{m1}, {m2}, {m3}});

  // First failing member's status wins, but every member is still commanded
  // (old groups commanded all members regardless of failures).
  EXPECT_EQ(group.SetSpeed(Rpm{50.0}), Status::kTimeout);
  EXPECT_EQ(IdsFor(log, "SetSpeed"), (std::vector<int>{1, 2, 3}));
}

TEST_F(ActuatorGroupTest, SimMotorOutOfRangeStillCommandsRemainingMembers) {
  // Real-implementation flavor of the same contract: the first sim clamps and
  // reports kOutOfRange, the second still receives the full command.
  SimMotorController small(100.0), big(5000.0);
  ASSERT_EQ(small.Connect(), Status::kOk);
  ASSERT_EQ(big.Connect(), Status::kOk);
  SpeedActuatorGroup group({{small}, {big}});

  EXPECT_EQ(group.SetSpeed(Rpm{200.0}), Status::kOutOfRange);
  EXPECT_DOUBLE_EQ(small.GetSpeed().value.value(), 100.0);  // clamped+reported
  EXPECT_DOUBLE_EQ(big.GetSpeed().value.value(), 200.0);    // still commanded
}

// ---- reads -------------------------------------------------------------------

TEST_F(ActuatorGroupTest, GetSpeedAveragesMembers) {
  std::vector<Call> log;
  ScriptedMotor m1(1, &log), m2(2, &log);
  m1.speed_reading = 100.0;
  m2.speed_reading = 200.0;
  SpeedActuatorGroup group({{m1}, {m2}});

  const Result<Rpm> avg = group.GetSpeed();
  ASSERT_TRUE(avg.ok());
  EXPECT_DOUBLE_EQ(avg.value.value(), 150.0);
  EXPECT_EQ(IdsFor(log, "GetSpeed"), (std::vector<int>{1, 2}));
}

TEST_F(ActuatorGroupTest, GetSpeedFailsInsteadOfAveragingErrorZeros) {
  std::vector<Call> log;
  ScriptedMotor m1(1, &log), m2(2, &log);
  m1.get_speed_status = Status::kNotConnected;
  m2.speed_reading = 200.0;
  SpeedActuatorGroup group({{m1}, {m2}});

  const Result<Rpm> avg = group.GetSpeed();
  EXPECT_FALSE(avg.ok());
  EXPECT_EQ(avg.status, Status::kNotConnected);
  // Fan-out is still complete: the healthy member was read too.
  EXPECT_EQ(IdsFor(log, "GetSpeed"), (std::vector<int>{1, 2}));
}

TEST_F(ActuatorGroupTest, EmptyGroupReadIsAnErrorNotDivideByZero) {
  SpeedActuatorGroup group({});
  EXPECT_EQ(group.size(), 0u);
  EXPECT_EQ(group.GetSpeed().status, Status::kInvalidArgument);
  EXPECT_EQ(group.SetSpeed(Rpm{10.0}), Status::kOk);  // vacuous broadcast
  EXPECT_EQ(group.Stop(), Status::kOk);
}

// ---- Stop semantics ----------------------------------------------------------

TEST_F(ActuatorGroupTest, StopFansToEveryMemberEvenAfterFailure) {
  std::vector<Call> log;
  ScriptedMotor m1(1, &log), m2(2, &log), m3(3, &log);
  m1.stop_status = Status::kIoError;
  SpeedActuatorGroup group({{m1}, {m2}, {m3}});

  EXPECT_EQ(group.Stop(), Status::kIoError);
  EXPECT_EQ(IdsFor(log, "Stop"), (std::vector<int>{1, 2, 3}));
}

TEST_F(ActuatorGroupTest, StopForcesSimMotorsToSafeState) {
  SimMotorController a, b;
  ASSERT_EQ(a.Connect(), Status::kOk);
  ASSERT_EQ(b.Connect(), Status::kOk);
  SpeedActuatorGroup group({{a}, {b}});

  ASSERT_EQ(group.SetSpeed(Rpm{500.0}), Status::kOk);
  EXPECT_EQ(group.Stop(), Status::kOk);
  EXPECT_DOUBLE_EQ(a.GetSpeed().value.value(), 0.0);
  EXPECT_DOUBLE_EQ(b.GetSpeed().value.value(), 0.0);
}

// ---- position group ----------------------------------------------------------

TEST_F(ActuatorGroupTest, PositionGroupMirrorsSpeedGroupSemantics) {
  std::vector<Call> log;
  ScriptedMotor m1(1, &log), m2(2, &log), m3(3, &log);
  m2.set_position_status = Status::kNotConnected;
  PositionActuatorGroup group({{m1}, {m2}, {m3}});
  ASSERT_EQ(group.size(), 3u);

  // Broadcast: full ordered fan-out despite the middle member failing.
  EXPECT_EQ(group.SetPosition(Radian{1.57}), Status::kNotConnected);
  EXPECT_EQ(IdsFor(log, "SetPosition"), (std::vector<int>{1, 2, 3}));
  for (const Call& c : log) EXPECT_DOUBLE_EQ(c.value, 1.57);
  log.clear();

  // Per-member fan-out maps by index; mismatch commands nothing.
  m2.set_position_status = Status::kOk;
  const std::vector<Radian> cmds{Radian{0.1}, Radian{0.2}, Radian{0.3}};
  EXPECT_EQ(group.SetPosition(cmds), Status::kOk);
  ASSERT_EQ(log.size(), 3u);
  EXPECT_DOUBLE_EQ(log[0].value, 0.1);
  EXPECT_DOUBLE_EQ(log[1].value, 0.2);
  EXPECT_DOUBLE_EQ(log[2].value, 0.3);
  EXPECT_EQ(group.SetPosition(cmds.data(), 2), Status::kInvalidArgument);
  log.clear();

  // Average read + failsafe Stop.
  m1.position_reading = 1.0;
  m2.position_reading = 2.0;
  m3.position_reading = 3.0;
  const Result<Radian> avg = group.GetPosition();
  ASSERT_TRUE(avg.ok());
  EXPECT_DOUBLE_EQ(avg.value.value(), 2.0);
  EXPECT_EQ(group.Stop(), Status::kOk);
  EXPECT_EQ(IdsFor(log, "Stop"), (std::vector<int>{1, 2, 3}));
}

// ---- instrumentation ----------------------------------------------------------

TEST_F(ActuatorGroupTest, FaultCounterCountsEveryFailedMemberCommand) {
  std::vector<Call> log;
  ScriptedMotor m1(1, &log), m2(2, &log), m3(3, &log);
  SpeedActuatorGroup group({{m1}, {m2}, {m3}});

  EXPECT_EQ(group.SetSpeed(Rpm{10.0}), Status::kOk);
  EXPECT_DOUBLE_EQ(FaultCount(), 0.0) << "clean commands must not count";

  m1.set_speed_status = Status::kTimeout;
  m3.set_speed_status = Status::kDeviceFault;
  EXPECT_EQ(group.SetSpeed(Rpm{10.0}), Status::kTimeout);
  EXPECT_DOUBLE_EQ(FaultCount(), 2.0) << "one count per failed member";

  m2.stop_status = Status::kIoError;
  EXPECT_EQ(group.Stop(), Status::kIoError);
  EXPECT_DOUBLE_EQ(FaultCount(), 3.0) << "Stop failures count too";
}

TEST_F(ActuatorGroupTest, WarnFiresOncePerFailureEpisode) {
  std::vector<Call> log;
  ScriptedMotor m1(1, &log), m2(2, &log);
  SpeedActuatorGroup group({{m1}, {m2}});

  m1.set_speed_status = Status::kTimeout;
  EXPECT_EQ(group.SetSpeed(Rpm{10.0}), Status::kTimeout);
  EXPECT_EQ(group.SetSpeed(Rpm{11.0}), Status::kTimeout);
  EXPECT_EQ(group.SetSpeed(Rpm{12.0}), Status::kTimeout);
  EXPECT_EQ(TakeWarnCount(), 1) << "one warn per episode, not per call";
  EXPECT_DOUBLE_EQ(FaultCount(), 3.0) << "counter still counts every fault";

  // A fully-clean command ends the episode; the next failure warns again.
  m1.set_speed_status = Status::kOk;
  EXPECT_EQ(group.SetSpeed(Rpm{13.0}), Status::kOk);
  m1.set_speed_status = Status::kTimeout;
  EXPECT_EQ(group.SetSpeed(Rpm{14.0}), Status::kTimeout);
  EXPECT_EQ(TakeWarnCount(), 2);
}

}  // namespace

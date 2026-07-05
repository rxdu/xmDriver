/*
 * test_health_telemetry.cpp
 *
 * Verifies the single DeviceHealth -> telemetry::HealthState mapping and the
 * transition-only discipline of hal::HealthReporter, using a minimal capture
 * binding (the custom-binding pattern from xmBase examples) so ReportHealth
 * calls become observable.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <mutex>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "xmdriver/hal/health_telemetry.hpp"

namespace tel = xmotion::telemetry;
using xmotion::hal::DeviceHealth;
using xmotion::hal::HealthReporter;
using xmotion::hal::ToTelemetryHealth;

namespace {

struct HealthRecord {
  std::string subsystem;
  tel::HealthState state;
  std::string detail;
};

std::mutex g_mtx;
std::vector<HealthRecord> g_records;

std::vector<HealthRecord> TakeRecords() {
  std::lock_guard<std::mutex> lk(g_mtx);
  std::vector<HealthRecord> out;
  out.swap(g_records);
  return out;
}

// Minimal capture binding: every pointer non-null (seam contract); only
// report_health records anything, the rest are inert.
const tel::Binding* CaptureBinding() {
  static tel::detail::CounterSlot counter;
  static tel::detail::GaugeSlot gauge;
  static tel::detail::HistogramSlot histogram;
  static tel::detail::SignalSlot signal;
  static tel::Binding b = [] {
    tel::Binding x{};
    x.abi_version = tel::kBindingAbiVersion;
    x.get_counter = [](std::string_view) { return &counter; };
    x.get_gauge = [](std::string_view) { return &gauge; };
    x.get_histogram = [](std::string_view) { return &histogram; };
    x.get_signal = [](std::string_view, std::size_t, std::size_t,
                      const char*) { return &signal; };
    x.intern_source = [](std::string_view) { return 1u; };
    x.should_log = [](tel::Severity) noexcept { return false; };
    x.set_level = [](tel::Severity) noexcept {};
    x.get_level = []() noexcept { return tel::Severity::kOff; };
    x.emit_event = [](std::uint32_t, tel::Severity, const char*,
                      const tel::detail::ArgPack&, tel::Context,
                      tel::Timestamp) noexcept {};
    x.emit_event_dyn = [](std::uint32_t, tel::Severity, const char*,
                          std::size_t, tel::Context,
                          tel::Timestamp) noexcept {};
    x.emit_span = [](const char*, tel::Context, tel::SpanId, tel::Timestamp,
                     tel::Timestamp, const tel::Context*,
                     std::uint8_t) noexcept {};
    x.emit_signal = [](tel::detail::SignalSlot*, const void*, std::size_t,
                       tel::Timestamp) noexcept {};
    x.report_health = [](const char* subsystem, tel::HealthState state,
                         const char* detail, tel::Context,
                         tel::Timestamp) noexcept {
      std::lock_guard<std::mutex> lk(g_mtx);
      g_records.push_back({subsystem, state, detail});
    };
    x.set_resource = [](std::string_view, std::string_view) {};
    return x;
  }();
  return &b;
}

class HealthTelemetryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(tel::InstallBinding(CaptureBinding()));
    TakeRecords();  // drop anything left over
  }
};

TEST(HealthMappingTest, MapsAllStatesOneToOne) {
  EXPECT_EQ(ToTelemetryHealth(DeviceHealth::State::kOk),
            tel::HealthState::kOk);
  EXPECT_EQ(ToTelemetryHealth(DeviceHealth::State::kDegraded),
            tel::HealthState::kDegraded);
  EXPECT_EQ(ToTelemetryHealth(DeviceHealth::State::kFault),
            tel::HealthState::kFault);
  EXPECT_EQ(ToTelemetryHealth(DeviceHealth::State::kDisconnected),
            tel::HealthState::kDisconnected);
}

TEST_F(HealthTelemetryTest, ReportsTransitionsOnlyWithMappedStates) {
  HealthReporter reporter("driver.test_device");

  // Initial state is kDisconnected: repeating it is not a transition.
  reporter.Update(DeviceHealth::State::kDisconnected);
  EXPECT_TRUE(TakeRecords().empty());

  // Full state walk, with repeats that must be suppressed.
  reporter.Update(DeviceHealth::State::kOk);
  reporter.Update(DeviceHealth::State::kOk);
  reporter.Update(DeviceHealth::State::kDegraded);
  reporter.Update(DeviceHealth::State::kDegraded);
  reporter.Update(DeviceHealth::State::kFault, "over_temp");
  reporter.Update(DeviceHealth::State::kDisconnected);

  const auto records = TakeRecords();
  ASSERT_EQ(records.size(), 4u);
  for (const auto& r : records) EXPECT_EQ(r.subsystem, "driver.test_device");
  EXPECT_EQ(records[0].state, tel::HealthState::kOk);
  EXPECT_EQ(records[1].state, tel::HealthState::kDegraded);
  EXPECT_EQ(records[2].state, tel::HealthState::kFault);
  EXPECT_EQ(records[2].detail, "over_temp");
  EXPECT_EQ(records[3].state, tel::HealthState::kDisconnected);
}

TEST_F(HealthTelemetryTest, DeviceHealthOverloadUsesStateDetail) {
  HealthReporter reporter("driver.test_device");
  reporter.Update(DeviceHealth{DeviceHealth::State::kDegraded, "stale 42ms"});

  const auto records = TakeRecords();
  ASSERT_EQ(records.size(), 1u);
  EXPECT_EQ(records[0].state, tel::HealthState::kDegraded);
  // Dynamic detail text must NOT cross the seam (literal contract): the
  // reporter substitutes the fixed per-state literal.
  EXPECT_EQ(records[0].detail, "degraded");
}

}  // namespace

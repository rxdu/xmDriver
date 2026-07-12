/*
 * test_sms_sts_array.cpp — SmsStsServoArray (shared-bus coordinator) on the
 * HAL, exercised over a pseudo-terminal with a fake multi-servo responder (no
 * hardware; same pty seam as the async_port loopback tests). The responder
 * speaks the SCS wire protocol: it acks writes, serves feedback reads from a
 * per-id register file, records every write, and stays silent for ids it does
 * not serve (absent-servo / timeout paths).
 *
 * Covers: factory registration + id-list parsing, id validation, round-robin
 * feedback polling into per-id snapshots, absent-id staleness + health,
 * per-id position/speed command frames (clamp, NaN, offset remap), the
 * sync-write batch (one bus transaction), command-ack timeout reporting,
 * failsafe stop-all, disconnect-to-safe-state, and actuator-group composition
 * via MemberRef.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <poll.h>
#include <pty.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "xmdriver/hal/actuator_group.hpp"
#include "xmdriver/hal/motor_factory.hpp"

#include "motor_waveshare/sms_sts_servo_array.hpp"

using namespace xmotion;
using namespace std::chrono_literals;

namespace {

// SCS register addresses used by the driver (mirror of the vendored library).
constexpr uint8_t kRegTorqueEnable = 40;
constexpr uint8_t kRegAcc = 41;
constexpr uint8_t kRegGoalSpeed = 46;
constexpr uint8_t kRegPresentPosition = 56;
constexpr uint8_t kFeedbackLen = 15;  // addr 56..70 snapshot

constexpr uint8_t kInstPing = 0x01;
constexpr uint8_t kInstRead = 0x02;
constexpr uint8_t kInstWrite = 0x03;
constexpr uint8_t kInstSyncWrite = 0x83;

struct RecordedWrite {
  uint8_t id;
  uint8_t addr;
  std::vector<uint8_t> data;
};

// Fake multi-servo bus behind a pty master: parses SCS request frames,
// acks/serves the ids in `served`, records every write (including sync-write
// fan-out), and ignores requests for other ids so the driver sees a timeout.
class FakeServoBus {
 public:
  explicit FakeServoBus(std::set<uint8_t> served) : served_(std::move(served)) {
    int slave_fd = -1;
    char name[128] = {0};
    if (openpty(&master_fd_, &slave_fd, name, nullptr, nullptr) != 0) {
      return;
    }
    slave_path_ = name;
    // Raw mode on both ends so the line discipline never echoes or cooks the
    // binary protocol before the driver reconfigures the slave.
    termios tio{};
    tcgetattr(slave_fd, &tio);
    cfmakeraw(&tio);
    tcsetattr(slave_fd, TCSANOW, &tio);
    slave_fd_ = slave_fd;  // keep open so the master never sees EIO
    running_.store(true);
    thread_ = std::thread([this] { Loop(); });
  }

  ~FakeServoBus() {
    running_.store(false);
    if (thread_.joinable()) thread_.join();
    if (master_fd_ >= 0) close(master_fd_);
    if (slave_fd_ >= 0) close(slave_fd_);
  }

  bool ok() const { return master_fd_ >= 0 && !slave_path_.empty(); }
  const std::string& slave_path() const { return slave_path_; }

  // Set the 15-byte feedback snapshot fields for one served id.
  void SetFeedback(uint8_t id, uint16_t position_steps, int16_t speed,
                   uint8_t temperature, uint8_t error = 0) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto& mem = feedback_[id];
    mem.assign(kFeedbackLen, 0);
    mem[0] = position_steps & 0xff;  // 56/57: present position (End=0: L,H)
    mem[1] = (position_steps >> 8) & 0xff;
    uint16_t spd = static_cast<uint16_t>(speed < 0 ? -speed : speed);
    if (speed < 0) spd |= (1u << 15);  // sign carried in bit 15
    mem[2] = spd & 0xff;  // 58/59: present speed
    mem[3] = (spd >> 8) & 0xff;
    mem[6] = 60;           // 62: voltage (0.1 V units)
    mem[7] = temperature;  // 63: temperature (deg C)
    error_[id] = error;
  }

  std::vector<RecordedWrite> writes() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return writes_;
  }

  void clear_writes() {
    std::lock_guard<std::mutex> lk(mtx_);
    writes_.clear();
  }

  // Block until `pred(writes)` holds or `timeout` elapses.
  bool WaitForWrites(
      const std::function<bool(const std::vector<RecordedWrite>&)>& pred,
      std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
      if (pred(writes())) return true;
      std::this_thread::sleep_for(1ms);
    }
    return pred(writes());
  }

 private:
  void Loop() {
    std::vector<uint8_t> buf;
    while (running_.load()) {
      pollfd pfd{master_fd_, POLLIN, 0};
      const int pr = poll(&pfd, 1, 1);
      if (pr <= 0) continue;
      uint8_t tmp[256];
      const ssize_t n = read(master_fd_, tmp, sizeof(tmp));
      if (n <= 0) continue;
      buf.insert(buf.end(), tmp, tmp + n);
      ParseAll(buf);
    }
  }

  void ParseAll(std::vector<uint8_t>& buf) {
    while (true) {
      // Hunt for the FF FF header.
      size_t start = 0;
      while (start + 1 < buf.size() &&
             !(buf[start] == 0xff && buf[start + 1] == 0xff)) {
        ++start;
      }
      if (start > 0) buf.erase(buf.begin(), buf.begin() + start);
      if (buf.size() < 4) return;
      const uint8_t len = buf[3];
      const size_t total = 4 + len;  // FF FF ID LEN + (len bytes incl chk)
      if (buf.size() < total) return;
      HandleFrame(std::vector<uint8_t>(buf.begin(), buf.begin() + total));
      buf.erase(buf.begin(), buf.begin() + total);
    }
  }

  void HandleFrame(const std::vector<uint8_t>& f) {
    const uint8_t id = f[2];
    const uint8_t inst = f[4];
    if (inst == kInstSyncWrite && id == 0xfe) {
      // FF FF FE LEN 83 ADDR NLEN (id data[nlen])... CHK
      const uint8_t addr = f[5];
      const uint8_t nlen = f[6];
      std::lock_guard<std::mutex> lk(mtx_);
      // Entries occupy [p, p+nlen]; the final byte of the frame is the
      // checksum and must not be consumed as entry data.
      for (size_t p = 7; p + nlen < f.size() - 1; p += 1 + nlen) {
        writes_.push_back(
            {f[p], addr, std::vector<uint8_t>(f.begin() + p + 1,
                                              f.begin() + p + 1 + nlen)});
      }
      return;  // sync-write is unacknowledged
    }
    if (served_.find(id) == served_.end()) return;  // absent servo: silence
    switch (inst) {
      case kInstPing:
        SendStatus(id, 0, {});
        break;
      case kInstWrite: {
        const uint8_t addr = f[5];
        {
          std::lock_guard<std::mutex> lk(mtx_);
          writes_.push_back(
              {id, addr, std::vector<uint8_t>(f.begin() + 6, f.end() - 1)});
        }
        SendStatus(id, 0, {});
        break;
      }
      case kInstRead: {
        const uint8_t addr = f[5];
        const uint8_t n = f[6];
        std::vector<uint8_t> data(n, 0);
        uint8_t err = 0;
        {
          std::lock_guard<std::mutex> lk(mtx_);
          const auto it = feedback_.find(id);
          if (addr == kRegPresentPosition && it != feedback_.end()) {
            for (uint8_t i = 0; i < n && i < it->second.size(); ++i) {
              data[i] = it->second[i];
            }
          }
          const auto eit = error_.find(id);
          if (eit != error_.end()) err = eit->second;
        }
        SendStatus(id, err, data);
        break;
      }
      default:
        SendStatus(id, 0, {});
        break;
    }
  }

  void SendStatus(uint8_t id, uint8_t err, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> out;
    out.push_back(0xff);
    out.push_back(0xff);
    out.push_back(id);
    out.push_back(static_cast<uint8_t>(data.size() + 2));
    out.push_back(err);
    out.insert(out.end(), data.begin(), data.end());
    uint8_t sum = 0;
    for (size_t i = 2; i < out.size(); ++i) sum += out[i];
    out.push_back(static_cast<uint8_t>(~sum));
    ssize_t written = write(master_fd_, out.data(), out.size());
    (void)written;
  }

  int master_fd_ = -1;
  int slave_fd_ = -1;
  std::string slave_path_;
  std::set<uint8_t> served_;
  std::thread thread_;
  std::atomic<bool> running_{false};

  mutable std::mutex mtx_;
  std::map<uint8_t, std::vector<uint8_t>> feedback_;
  std::map<uint8_t, uint8_t> error_;
  std::vector<RecordedWrite> writes_;
};

SmsStsServoArray::Config TestConfig(const std::string& bus) {
  SmsStsServoArray::Config cfg;
  cfg.bus = bus;
  cfg.ids = {1, 2, 3};
  cfg.baud = 1000000;
  cfg.max_speed = 2400.0;
  cfg.position_offset_deg = 0.0;
  cfg.io_timeout_ms = 20;  // keep absent-id stalls short in CI
  return cfg;
}

// Wait until `pred()` holds or `timeout` elapses.
bool WaitFor(const std::function<bool()>& pred,
             std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (pred()) return true;
    std::this_thread::sleep_for(2ms);
  }
  return pred();
}

uint16_t StepsFromWrite(const RecordedWrite& w) {
  // WritePosEx payload at kRegAcc: [acc, posL, posH, 0, 0, spdL, spdH]
  return static_cast<uint16_t>(w.data[1]) |
         (static_cast<uint16_t>(w.data[2]) << 8);
}

}  // namespace

TEST(SmsStsServoArrayTest, RegistersWithFactoryAndParsesParams) {
  ASSERT_TRUE(RegisterSmsStsServoArray());
  EXPECT_TRUE(hal::MotorFactory::Instance().IsRegistered("sms_sts_array"));

  hal::MotorConfig cfg;
  cfg.type = "sms_sts_array";
  cfg.bus = "/dev/ttyUSB0";
  cfg.params["ids"] = "1,2,3,4";
  cfg.params["baud"] = "1000000";
  cfg.params["position_offset_deg"] = "180";
  auto m = hal::MotorFactory::Instance().Create(cfg);
  ASSERT_NE(m, nullptr);
  EXPECT_FALSE(m->IsConnected());
  EXPECT_EQ(m->Health().state, hal::DeviceHealth::State::kDisconnected);

  auto* array = dynamic_cast<SmsStsServoArray*>(m.get());
  ASSERT_NE(array, nullptr);
  EXPECT_EQ(array->ids(), (std::vector<std::uint8_t>{1, 2, 3, 4}));
}

TEST(SmsStsServoArrayTest, FactoryRejectsMalformedParams) {
  RegisterSmsStsServoArray();
  hal::MotorConfig cfg;
  cfg.type = "sms_sts_array";
  cfg.bus = "/dev/ttyUSB0";
  EXPECT_EQ(hal::MotorFactory::Instance().Create(cfg), nullptr);  // no ids

  cfg.params["ids"] = "1,abc";
  EXPECT_EQ(hal::MotorFactory::Instance().Create(cfg), nullptr);

  cfg.params["ids"] = "1,2";
  cfg.params["baud"] = "fast";
  EXPECT_EQ(hal::MotorFactory::Instance().Create(cfg), nullptr);

  cfg.params["baud"] = "1000000";
  cfg.params["position_offset_deg"] = "many";
  EXPECT_EQ(hal::MotorFactory::Instance().Create(cfg), nullptr);
}

TEST(SmsStsServoArrayTest, ConnectRejectsBadIdLists) {
  {
    auto cfg = TestConfig("/dev/null");
    cfg.ids = {};
    SmsStsServoArray m(cfg);
    EXPECT_EQ(m.Connect(), hal::Status::kInvalidArgument);
  }
  {
    auto cfg = TestConfig("/dev/null");
    cfg.ids = {1, 1};
    SmsStsServoArray m(cfg);
    EXPECT_EQ(m.Connect(), hal::Status::kInvalidArgument);
  }
  {
    auto cfg = TestConfig("/dev/null");
    cfg.ids = {1, 0xfe};  // broadcast id is reserved
    SmsStsServoArray m(cfg);
    EXPECT_EQ(m.Connect(), hal::Status::kInvalidArgument);
  }
}

TEST(SmsStsServoArrayTest, ConnectFailsOnUnopenablePort) {
  auto cfg = TestConfig("/nonexistent/tty");
  SmsStsServoArray m(cfg);
  EXPECT_EQ(m.Connect(), hal::Status::kIoError);
  EXPECT_FALSE(m.IsConnected());
}

TEST(SmsStsServoArrayTest, DisconnectedCommandsAndReadsRejected) {
  SmsStsServoArray m(TestConfig("/dev/ttyUSB0"));
  EXPECT_EQ(m.SetPosition(1, hal::Radian{1.0}), hal::Status::kNotConnected);
  EXPECT_EQ(m.SetSpeed(1, hal::Rpm{100.0}), hal::Status::kNotConnected);
  EXPECT_EQ(m.SetPositions({hal::Radian{1}, hal::Radian{1}, hal::Radian{1}}),
            hal::Status::kNotConnected);
  EXPECT_EQ(m.Stop(), hal::Status::kNotConnected);
  EXPECT_EQ(m.EnableTorque(1, true), hal::Status::kNotConnected);
  EXPECT_EQ(m.GetPosition(1).status, hal::Status::kNotConnected);
  EXPECT_EQ(m.GetSpeed(1).status, hal::Status::kNotConnected);
  EXPECT_EQ(m.GetState(1).status, hal::Status::kNotConnected);
}

TEST(SmsStsServoArrayTest, PollsFeedbackPerIdAndFlagsAbsentId) {
  FakeServoBus bus({1, 2});  // id 3 is registered but absent from the bus
  ASSERT_TRUE(bus.ok());
  bus.SetFeedback(1, /*position_steps=*/2047, /*speed=*/100,
                  /*temperature=*/35);
  bus.SetFeedback(2, /*position_steps=*/1023, /*speed=*/-200,
                  /*temperature=*/40);

  SmsStsServoArray m(TestConfig(bus.slave_path()));
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  EXPECT_TRUE(m.IsConnected());

  // The poll thread round-robins ids into per-id snapshots.
  ASSERT_TRUE(WaitFor([&] { return m.GetPosition(1).ok(); }, 2000ms));
  ASSERT_TRUE(WaitFor([&] { return m.GetPosition(2).ok(); }, 2000ms));

  auto p1 = m.GetPosition(1);
  EXPECT_NEAR(p1.value.value(), 2047.0 / 4095.0 * 2.0 * M_PI, 1e-2);
  auto s2 = m.GetSpeed(2);
  ASSERT_TRUE(s2.ok());
  EXPECT_DOUBLE_EQ(s2.value.value(), -200.0);

  auto st1 = m.GetState(1);
  ASSERT_TRUE(st1.ok());
  EXPECT_DOUBLE_EQ(st1.value.temperature, 35.0);
  EXPECT_DOUBLE_EQ(st1.value.voltage, 60.0);

  // The absent id never becomes fresh and degrades only its own health.
  EXPECT_EQ(m.GetPosition(3).status, hal::Status::kTimeout);
  EXPECT_EQ(m.Health(1).state, hal::DeviceHealth::State::kOk);
  EXPECT_EQ(m.Health(3).state, hal::DeviceHealth::State::kDegraded);
  EXPECT_EQ(m.Health().state, hal::DeviceHealth::State::kDegraded);

  m.Disconnect();
}

TEST(SmsStsServoArrayTest, UnknownIdRejected) {
  FakeServoBus bus({1, 2, 3});
  ASSERT_TRUE(bus.ok());
  SmsStsServoArray m(TestConfig(bus.slave_path()));
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  EXPECT_EQ(m.SetPosition(9, hal::Radian{1.0}), hal::Status::kInvalidArgument);
  EXPECT_EQ(m.SetSpeed(9, hal::Rpm{10.0}), hal::Status::kInvalidArgument);
  EXPECT_EQ(m.Stop(9), hal::Status::kInvalidArgument);
  EXPECT_EQ(m.EnableTorque(9, true), hal::Status::kInvalidArgument);
  EXPECT_EQ(m.GetPosition(9).status, hal::Status::kInvalidArgument);
  EXPECT_EQ(m.GetState(9).status, hal::Status::kInvalidArgument);
  EXPECT_EQ(m.Member(9), nullptr);
  m.Disconnect();
}

TEST(SmsStsServoArrayTest, SetPositionWritesGoalFrameWithOffsetRemap) {
  FakeServoBus bus({1, 2, 3});
  ASSERT_TRUE(bus.ok());
  auto cfg = TestConfig(bus.slave_path());
  cfg.position_offset_deg = 180.0;  // the swervebot steering remap
  SmsStsServoArray m(cfg);
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  bus.clear_writes();

  // pi/2 rad = 90 deg; +180 offset => 270 deg => 270/360*4095 = 3071 steps.
  EXPECT_EQ(m.SetPosition(1, hal::Radian{M_PI / 2.0}), hal::Status::kOk);
  ASSERT_TRUE(bus.WaitForWrites(
      [](const std::vector<RecordedWrite>& w) { return !w.empty(); }, 500ms));
  bool found = false;
  for (const auto& w : bus.writes()) {
    if (w.id == 1 && w.addr == kRegAcc && w.data.size() == 7) {
      EXPECT_EQ(StepsFromWrite(w), 3071);
      found = true;
    }
  }
  EXPECT_TRUE(found) << "no WritePosEx goal frame recorded for id 1";

  // Non-finite is rejected before the wire; out-of-range is clamped+reported.
  EXPECT_EQ(m.SetPosition(1, hal::Radian{std::nan("")}),
            hal::Status::kInvalidArgument);
  EXPECT_EQ(m.SetPosition(1, hal::Radian{100.0}), hal::Status::kOutOfRange);
  m.Disconnect();
}

TEST(SmsStsServoArrayTest, CommandToAbsentIdReportsIoError) {
  FakeServoBus bus({1, 2});  // id 3 never acks
  ASSERT_TRUE(bus.ok());
  SmsStsServoArray m(TestConfig(bus.slave_path()));
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  EXPECT_EQ(m.SetPosition(3, hal::Radian{1.0}), hal::Status::kIoError);
  EXPECT_EQ(m.SetSpeed(3, hal::Rpm{100.0}), hal::Status::kIoError);
  m.Disconnect();
}

TEST(SmsStsServoArrayTest, BatchSyncWriteIsOneTransactionForAllIds) {
  FakeServoBus bus({1, 2, 3});
  ASSERT_TRUE(bus.ok());
  SmsStsServoArray m(TestConfig(bus.slave_path()));
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  bus.clear_writes();

  // 90 / 180 / 270 deg -> 1023 / 2047 / 3071 steps.
  EXPECT_EQ(m.SetPositions({hal::Radian{M_PI / 2.0}, hal::Radian{M_PI},
                            hal::Radian{3.0 * M_PI / 2.0}}),
            hal::Status::kOk);
  ASSERT_TRUE(bus.WaitForWrites(
      [](const std::vector<RecordedWrite>& w) { return w.size() >= 3; },
      500ms));
  const auto writes = bus.writes();
  ASSERT_EQ(writes.size(), 3u);
  const uint16_t expect_steps[] = {1023, 2047, 3071};
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(writes[i].id, i + 1);  // registration order preserved
    EXPECT_EQ(writes[i].addr, kRegAcc);
    ASSERT_EQ(writes[i].data.size(), 7u);
    EXPECT_EQ(StepsFromWrite(writes[i]), expect_steps[i]);
  }
  m.Disconnect();
}

TEST(SmsStsServoArrayTest, BatchValidatesBeforeAnythingHitsTheWire) {
  FakeServoBus bus({1, 2, 3});
  ASSERT_TRUE(bus.ok());
  SmsStsServoArray m(TestConfig(bus.slave_path()));
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  bus.clear_writes();

  // Size mismatch: nothing commanded.
  EXPECT_EQ(m.SetPositions({hal::Radian{1.0}}), hal::Status::kInvalidArgument);
  // Any non-finite entry: nothing commanded (sync-write is indivisible).
  EXPECT_EQ(m.SetPositions({hal::Radian{1.0}, hal::Radian{std::nan("")},
                            hal::Radian{1.0}}),
            hal::Status::kInvalidArgument);
  std::this_thread::sleep_for(50ms);
  EXPECT_TRUE(bus.writes().empty());
  m.Disconnect();
}

TEST(SmsStsServoArrayTest, StopFansOutZeroSpeedToEveryId) {
  FakeServoBus bus({1, 2, 3});
  ASSERT_TRUE(bus.ok());
  SmsStsServoArray m(TestConfig(bus.slave_path()));
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  bus.clear_writes();

  EXPECT_EQ(m.Stop(), hal::Status::kOk);
  // WriteSpe(id, 0) writes the goal-speed register with 0 for every id.
  for (uint8_t id = 1; id <= 3; ++id) {
    bool found = false;
    for (const auto& w : bus.writes()) {
      if (w.id == id && w.addr == kRegGoalSpeed && w.data.size() == 2 &&
          w.data[0] == 0 && w.data[1] == 0) {
        found = true;
      }
    }
    EXPECT_TRUE(found) << "no zero-speed write for id "
                       << static_cast<int>(id);
  }
  m.Disconnect();
}

TEST(SmsStsServoArrayTest, EnableTorquePerIdAndAll) {
  FakeServoBus bus({1, 2, 3});
  ASSERT_TRUE(bus.ok());
  SmsStsServoArray m(TestConfig(bus.slave_path()));
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  bus.clear_writes();

  EXPECT_EQ(m.EnableTorque(2, true), hal::Status::kOk);
  EXPECT_EQ(m.EnableTorqueAll(false), hal::Status::kOk);
  const auto writes = bus.writes();
  int enable_writes = 0;
  int disable_writes = 0;
  for (const auto& w : writes) {
    if (w.addr == kRegTorqueEnable && w.data.size() == 1) {
      if (w.data[0] == 1) ++enable_writes;
      if (w.data[0] == 0) ++disable_writes;
    }
  }
  EXPECT_EQ(enable_writes, 1);
  EXPECT_EQ(disable_writes, 3);
  m.Disconnect();
}

TEST(SmsStsServoArrayTest, DisconnectCommandsSafeStateBeforeClosing) {
  FakeServoBus bus({1, 2, 3});
  ASSERT_TRUE(bus.ok());
  SmsStsServoArray m(TestConfig(bus.slave_path()));
  ASSERT_EQ(m.Connect(), hal::Status::kOk);
  bus.clear_writes();

  m.Disconnect();
  EXPECT_FALSE(m.IsConnected());
  // The failsafe zero-speed writes were issued on the way out.
  int zero_speed_writes = 0;
  for (const auto& w : bus.writes()) {
    if (w.addr == kRegGoalSpeed && w.data.size() == 2 && w.data[0] == 0 &&
        w.data[1] == 0) {
      ++zero_speed_writes;
    }
  }
  EXPECT_EQ(zero_speed_writes, 3);
  EXPECT_EQ(m.Health().state, hal::DeviceHealth::State::kDisconnected);
}

TEST(SmsStsServoArrayTest, MemberRefsComposeIntoActuatorGroups) {
  FakeServoBus bus({1, 2, 3});
  ASSERT_TRUE(bus.ok());
  SmsStsServoArray m(TestConfig(bus.slave_path()));
  ASSERT_EQ(m.Connect(), hal::Status::kOk);

  auto* m1 = m.Member(1);
  auto* m2 = m.Member(2);
  ASSERT_NE(m1, nullptr);
  ASSERT_NE(m2, nullptr);

  bus.clear_writes();
  hal::PositionActuatorGroup steering({{*m1}, {*m2}});
  EXPECT_EQ(steering.SetPosition(hal::Radian{M_PI}), hal::Status::kOk);
  const auto writes = bus.writes();
  int goal_frames = 0;
  for (const auto& w : writes) {
    if (w.addr == kRegAcc && w.data.size() == 7) {
      EXPECT_EQ(StepsFromWrite(w), 2047);  // 180 deg
      ++goal_frames;
    }
  }
  EXPECT_EQ(goal_frames, 2);
  EXPECT_EQ(steering.Stop(), hal::Status::kOk);
  m.Disconnect();
}

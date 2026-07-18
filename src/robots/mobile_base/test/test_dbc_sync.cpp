/*
 * DBC sync (spec §13): the machine-readable dictionary docs/mobile_base.dbc and
 * the codec are the same source of truth. This parses the DBC and asserts every
 * frame ID and the representative scale factors match the core.hpp / profile
 * constants, so an edit to one that is not mirrored in the other fails CI.
 *
 * (The DBC cannot express the E2E counter/CRC semantics of §4; those live only
 * in e2e.hpp and are covered by test_e2e.cpp.)
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "mobile_base/core.hpp"
#include "mobile_base/profiles/swerve_v1.hpp"

using namespace xmotion::mobile_base;
namespace sw = xmotion::mobile_base::profiles::swerve_v1;

namespace {
struct Dbc {
  std::map<std::string, std::uint32_t> msg_id;                   // name -> id
  std::map<std::pair<std::string, std::string>, double> factor;  // (msg,sig)->factor
};

// Minimal parser: enough of the DBC grammar to read BO_ (message) ids and the
// SG_ (signal) linear factor. Not a general DBC reader.
Dbc LoadDbc(const std::string& path) {
  Dbc d;
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open()) << "cannot open DBC: " << path;
  std::string line, cur;
  while (std::getline(in, line)) {
    std::istringstream ss(line);
    std::string tok;
    ss >> tok;
    if (tok == "BO_") {
      std::uint32_t id = 0;
      std::string name;
      ss >> id >> name;
      if (!name.empty() && name.back() == ':') name.pop_back();
      d.msg_id[name] = id;
      cur = name;
    } else if (tok == "SG_" && !cur.empty()) {
      std::string sig;
      ss >> sig;
      const auto lp = line.find('(');
      const auto comma = line.find(',', lp);
      if (lp != std::string::npos && comma != std::string::npos) {
        d.factor[{cur, sig}] = std::stod(line.substr(lp + 1, comma - lp - 1));
      }
    }
  }
  return d;
}
}  // namespace

TEST(MobileBaseDbcSync, CoreMessageIds) {
  const Dbc d = LoadDbc(MOBILE_BASE_DBC_PATH);
  EXPECT_EQ(d.msg_id.at("CMD_TWIST"), kCmdTwist);
  EXPECT_EQ(d.msg_id.at("CMD_MODE"), kCmdMode);
  EXPECT_EQ(d.msg_id.at("CMD_ESTOP"), kCmdEStop);
  EXPECT_EQ(d.msg_id.at("CMD_HEARTBEAT"), kCmdHeartbeat);
  EXPECT_EQ(d.msg_id.at("STATE_STATUS"), kStateStatus);
  EXPECT_EQ(d.msg_id.at("STATE_ODOM_TWIST"), kStateOdomTwist);
  EXPECT_EQ(d.msg_id.at("STATE_ODOM_POSE"), kStateOdomPose);
  EXPECT_EQ(d.msg_id.at("STATE_CAPABILITIES"), kStateCapabilities);
  EXPECT_EQ(d.msg_id.at("STATE_LIMITS"), kStateLimits);
  EXPECT_EQ(d.msg_id.at("STATE_BATTERY"), kStateBattery);
  EXPECT_EQ(d.msg_id.at("STATE_FAULTS"), kStateFaults);
}

TEST(MobileBaseDbcSync, SwerveModuleIdsInExtensionRange) {
  const Dbc d = LoadDbc(MOBILE_BASE_DBC_PATH);
  const char* names[] = {"STATE_MODULE_FR", "STATE_MODULE_FL",
                         "STATE_MODULE_RL", "STATE_MODULE_RR"};
  for (std::uint8_t i = 0; i < sw::kNumModules; ++i) {
    EXPECT_EQ(d.msg_id.at(names[i]), sw::ModuleId(i));
    EXPECT_TRUE(IsModelStateId(d.msg_id.at(names[i])));
  }
}

TEST(MobileBaseDbcSync, ScaleFactorsMatchCodec) {
  const Dbc d = LoadDbc(MOBILE_BASE_DBC_PATH);
  // DBC factor is physical-per-LSB; the codec multiplies SI by kXxxScale, so the
  // two are reciprocals. A drift in either side breaks this.
  EXPECT_NEAR(d.factor.at({"CMD_TWIST", "vx"}), 1.0 / kVelScale, 1e-12);
  EXPECT_NEAR(d.factor.at({"CMD_TWIST", "wz"}), 1.0 / kAngVelScale, 1e-12);
  EXPECT_NEAR(d.factor.at({"STATE_ODOM_POSE", "dx"}), 1.0 / kPoseScale, 1e-12);
  EXPECT_NEAR(d.factor.at({"STATE_ODOM_POSE", "dtheta"}), 1.0 / kThetaScale, 1e-12);
  EXPECT_NEAR(d.factor.at({"STATE_BATTERY", "voltage"}), 1.0 / kVoltScale, 1e-12);
  EXPECT_NEAR(d.factor.at({"STATE_BATTERY", "current"}), 1.0 / kAmpScale, 1e-12);
  EXPECT_NEAR(d.factor.at({"STATE_MODULE_FR", "angle"}), 1.0 / kAngleScale, 1e-12);
}

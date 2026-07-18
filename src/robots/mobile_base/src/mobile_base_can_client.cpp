/*
 * mobile_base_can_client.cpp — MobileBaseCanClient implementation (spec §6-§9).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */
#include "mobile_base/mobile_base_can_client.hpp"

#include <utility>

#include "async_port/async_can.hpp"

namespace xmotion {

using namespace mobile_base;  // protocol names; hal:: stays qualified

MobileBaseCanClient::MobileBaseCanClient(Config config)
    : cfg_(std::move(config)),
      status_fresh_(cfg_.state_timeout),
      odom_twist_fresh_(cfg_.state_timeout),
      odom_pose_fresh_(cfg_.state_timeout),
      caps_fresh_(cfg_.state_timeout),
      limits_fresh_(cfg_.state_timeout),
      battery_fresh_(cfg_.state_timeout),
      faults_fresh_(cfg_.state_timeout) {}

MobileBaseCanClient::~MobileBaseCanClient() { Disconnect(); }

hal::Status MobileBaseCanClient::Connect() {
  return Connect(std::make_shared<AsyncCAN>(cfg_.can_interface));
}

hal::Status MobileBaseCanClient::Connect(std::shared_ptr<CanInterface> can) {
  if (!can) return hal::Status::kInvalidArgument;
  can_ = std::move(can);
  can_->SetReceiveCallback([this](const CanFrame& f) { OnFrame(f); });
  can_->SetErrorCallback([this](TransportStatus) { connected_.store(false); });
  if (!can_->Open()) {
    can_.reset();
    return hal::Status::kIoError;
  }
  connected_.store(true);
  return hal::Status::kOk;
}

void MobileBaseCanClient::Disconnect() {
  connected_.store(false);
  if (can_) {
    can_->Close();
    can_.reset();
  }
  std::lock_guard<std::mutex> lk(state_mtx_);
  status_fresh_.Reset();
  odom_twist_fresh_.Reset();
  odom_pose_fresh_.Reset();
  caps_fresh_.Reset();
  limits_fresh_.Reset();
  battery_fresh_.Reset();
  faults_fresh_.Reset();
}

bool MobileBaseCanClient::IsConnected() const { return connected_.load(); }

hal::DeviceHealth MobileBaseCanClient::Health() const {
  const bool conn = connected_.load();
  std::lock_guard<std::mutex> lk(state_mtx_);
  hal::DeviceHealth h = hal::HealthFromFreshness(conn, status_fresh_);
  // A fresh STATUS can still carry a fault or e-stop — surface it above "ok".
  if (conn && status_fresh_.IsFresh()) {
    if (status_.mode == ReportedMode::kEStop ||
        (status_.flags & kFlagEStopLatched)) {
      h.state = hal::DeviceHealth::State::kFault;
      h.detail = "e-stop engaged";
    } else if (status_.faults != 0) {
      h.state = hal::DeviceHealth::State::kFault;
      h.detail = "base reports faults";
    } else if (status_.flags & kFlagBusDegraded) {
      h.state = hal::DeviceHealth::State::kDegraded;
      h.detail = "bus degraded";
    }
  }
  return h;
}

// ---- commands --------------------------------------------------------------
hal::Status MobileBaseCanClient::SetTwist(double vx, double vy, double wz) {
  return Send(Encode(TwistCommand{vx, vy, wz, twist_ctr_.fetch_add(1)}));
}
hal::Status MobileBaseCanClient::SetMode(ModeRequest mode) {
  return Send(Encode(ModeCommand{mode, mode_ctr_.fetch_add(1)}));
}
hal::Status MobileBaseCanClient::EngageEStop() {
  return Send(
      Encode(EStopCommand{EStopAction::kEngage, 0, estop_ctr_.fetch_add(1)}));
}
hal::Status MobileBaseCanClient::ClearEStop() {
  return Send(Encode(EStopCommand{EStopAction::kClear, kEStopClearKey,
                                  estop_ctr_.fetch_add(1)}));
}
hal::Status MobileBaseCanClient::SendHeartbeat() {
  return Send(
      Encode(HeartbeatCommand{cfg_.protocol_version, hb_ctr_.fetch_add(1)}));
}

// ---- state reads -----------------------------------------------------------
hal::Result<StatusState> MobileBaseCanClient::ReadStatus() const {
  return Read(status_, status_fresh_);
}
hal::Result<OdomTwistState> MobileBaseCanClient::ReadOdomTwist() const {
  return Read(odom_twist_, odom_twist_fresh_);
}
hal::Result<OdomPoseState> MobileBaseCanClient::ReadOdomPose() const {
  return Read(odom_pose_, odom_pose_fresh_);
}
hal::Result<CapabilitiesState> MobileBaseCanClient::ReadCapabilities() const {
  return Read(caps_, caps_fresh_);
}
hal::Result<LimitsState> MobileBaseCanClient::ReadLimits() const {
  return Read(limits_, limits_fresh_);
}
hal::Result<BatteryState> MobileBaseCanClient::ReadBattery() const {
  return Read(battery_, battery_fresh_);
}
hal::Result<FaultState> MobileBaseCanClient::ReadFaults() const {
  return Read(faults_, faults_fresh_);
}

// ---- internals -------------------------------------------------------------
hal::Status MobileBaseCanClient::Send(const CanFrame& f) {
  if (!can_ || !connected_.load()) return hal::Status::kNotConnected;
  switch (can_->SendFrame(f)) {
    case TransportStatus::kOk:
      return hal::Status::kOk;
    case TransportStatus::kNotOpen:
      return hal::Status::kNotConnected;
    case TransportStatus::kQueueFull:
    case TransportStatus::kIoError:
      return hal::Status::kIoError;
    case TransportStatus::kInvalidArgument:
      return hal::Status::kInvalidArgument;
  }
  return hal::Status::kIoError;
}

void MobileBaseCanClient::OnFrame(const CanFrame& f) {
  switch (f.id) {
    case kStateStatus:
      if (auto s = DecodeStatus(f)) Store(status_, *s, status_fresh_);
      return;
    case kStateOdomTwist:
      if (auto s = DecodeOdomTwist(f)) Store(odom_twist_, *s, odom_twist_fresh_);
      return;
    case kStateOdomPose:
      if (auto s = DecodeOdomPose(f)) Store(odom_pose_, *s, odom_pose_fresh_);
      return;
    case kStateCapabilities:
      if (auto s = DecodeCapabilities(f)) Store(caps_, *s, caps_fresh_);
      return;
    case kStateLimits:
      if (auto s = DecodeLimits(f)) Store(limits_, *s, limits_fresh_);
      return;
    case kStateBattery:
      if (auto s = DecodeBattery(f)) Store(battery_, *s, battery_fresh_);
      return;
    case kStateFaults:
      if (auto s = DecodeFaults(f)) Store(faults_, *s, faults_fresh_);
      return;
    default:
      // Model-profile extension frames go to the profile decoder verbatim.
      if (IsModelStateId(f.id) || IsModelCmdId(f.id)) {
        if (model_cb_) model_cb_(f);
      }
      return;
  }
}

}  // namespace xmotion

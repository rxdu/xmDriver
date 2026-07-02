/*
 * vesc_can_interface.cpp
 *
 * Created on 6/30/22 10:21 PM
 * Description:
 *
 * Copyright (c) 2022 Ruixiang Du (rdu)
 */

#include "motor_vesc/vesc_can_interface.hpp"

#include <functional>

#include "async_port/async_can.hpp"
#include "xmsigma/logging/xlogger.hpp"

#include "motor_vesc/vesc_cmd_packet.hpp"
#include "motor_vesc/vesc_status_packet.hpp"

namespace xmotion {
namespace {
// Send a command frame, log any transport failure, and hand the status back so
// the caller can map it onto hal::Status. A send is never silently dropped.
TransportStatus SendCmd(const std::shared_ptr<CanInterface> &can,
                        uint8_t vesc_id, const CanFrame &frame,
                        const char *what) {
  if (can == nullptr || !can->IsOpened()) return TransportStatus::kNotOpen;
  const TransportStatus st = can->SendFrame(frame);
  if (st != TransportStatus::kOk) {
    XLOG_WARN("VescCanInterface: {} send failed on vesc {}: {}", what,
              static_cast<int>(vesc_id), ToString(st));
  }
  return st;
}
}  // namespace

bool VescCanInterface::Connect(const std::string &can, uint8_t vesc_id) {
  vesc_id_ = vesc_id;
  can_ = std::make_shared<AsyncCAN>(can);
  can_->SetReceiveCallback(
      std::bind(&VescCanInterface::HandleCanFrame, this, std::placeholders::_1));
  // Observe async bus faults: log, then forward to any owner-installed handler
  // so the driver can mark itself degraded/faulted.
  can_->SetErrorCallback([this](TransportStatus reason) {
    XLOG_ERROR("VescCanInterface: CAN bus fault on vesc {}: {}",
               static_cast<int>(vesc_id_), ToString(reason));
    if (error_callback_) error_callback_(reason);
  });
  return can_->Open();
}

void VescCanInterface::Disconnect() {
  if (can_ && can_->IsOpened()) {
    can_->Close();
  }
}

uint8_t VescCanInterface::GetVescId() const {
  return vesc_id_;
}

void VescCanInterface::SetStateUpdatedCallback(StateUpdatedCallback cb) {
  state_updated_callback_ = cb;
}

void VescCanInterface::SetErrorCallback(ErrorCallback cb) {
  error_callback_ = cb;
}

StampedVescState VescCanInterface::GetLastState() const {
  std::lock_guard<std::mutex> lock(state_mtx_);
  return stamped_state_;
}

void VescCanInterface::HandleCanFrame(const CanFrame &frame) {
  // The transport already delivers the masked 29-bit identifier (no EFF flag).
  const uint32_t can_id = frame.id;

  // Every VESC status frame carries 8 data bytes. A matching id with a shorter
  // DLC is a malformed / foreign frame — reject it rather than parse stale or
  // kernel-pad trailing bytes into the state.
  if (frame.dlc < 8) return;

  bool updated = false;
  {
    std::lock_guard<std::mutex> lock(state_mtx_);

    if (can_id == (VescFrame::VescStatus1FrameId | vesc_id_)) {
      auto pkt = VescStatus1Packet(frame);
      stamped_state_.field = VescStateUpdatedField::kStatus1;
      stamped_state_.state.speed = pkt.GetRpm();
      stamped_state_.state.current_motor = pkt.GetCurrent();
      stamped_state_.state.duty_cycle = pkt.GetDuty();
      updated = true;
    } else if (can_id == (VescFrame::VescStatus2FrameId | vesc_id_)) {
      auto pkt = VescStatus2Packet(frame);
      stamped_state_.field = VescStateUpdatedField::kStatus2;
      stamped_state_.state.charge_drawn = pkt.GetAmpHours();
      stamped_state_.state.charge_regen = pkt.GetAmpHoursCharged();
      updated = true;
    } else if (can_id == (VescFrame::VescStatus3FrameId | vesc_id_)) {
      auto pkt = VescStatus3Packet(frame);
      stamped_state_.field = VescStateUpdatedField::kStatus3;
      stamped_state_.state.energy_drawn = pkt.GetWattHours();
      stamped_state_.state.energy_regen = pkt.GetWattHoursCharged();
      updated = true;
    } else if (can_id == (VescFrame::VescStatus4FrameId | vesc_id_)) {
      auto pkt = VescStatus4Packet(frame);
      stamped_state_.field = VescStateUpdatedField::kStatus4;
      stamped_state_.state.temperature_pcb = pkt.GetTempFET();
      stamped_state_.state.current_input = pkt.GetCurrentIn();
      updated = true;
    } else if (can_id == (VescFrame::VescStatus5FrameId | vesc_id_)) {
      auto pkt = VescStatus5Packet(frame);
      stamped_state_.field = VescStateUpdatedField::kStatus5;
      stamped_state_.state.displacement = pkt.GetTachoValue();
      stamped_state_.state.voltage_input = pkt.GetVoltageIn();
      updated = true;
    }

    // Only stamp the state when a frame was actually consumed — don't advance
    // the timestamp (or publish below) for an unmatched/foreign id.
    if (updated) stamped_state_.time = Now();  // xmsigma/types/time.hpp
  }

  if (updated && state_updated_callback_) {
    state_updated_callback_(stamped_state_);
  }
}

// TODO
void VescCanInterface::RequestFwVersion() {

}

// TODO
void VescCanInterface::RequestState() {

}

// TODO
void VescCanInterface::RequestImuData() {

}

TransportStatus VescCanInterface::SetDutyCycle(double duty_cycle) {
  return SendCmd(can_, vesc_id_,
                 VescSetDutyCycleCmdPacket(vesc_id_, duty_cycle).GetCanFrame(),
                 "SetDutyCycle");
}

TransportStatus VescCanInterface::SetCurrent(double current) {
  return SendCmd(can_, vesc_id_,
                 VescSetCurrentCmdPacket(vesc_id_, current).GetCanFrame(),
                 "SetCurrent");
}

TransportStatus VescCanInterface::SetBrake(double brake) {
  return SendCmd(can_, vesc_id_,
                 VescSetCurrentBrakeCmdPacket(vesc_id_, brake).GetCanFrame(),
                 "SetBrake");
}

TransportStatus VescCanInterface::SetSpeed(double speed) {
  return SendCmd(can_, vesc_id_,
                 VescSetRpmCmdPacket(vesc_id_, speed).GetCanFrame(), "SetSpeed");
}

TransportStatus VescCanInterface::SetPosition(double position) {
  return SendCmd(can_, vesc_id_,
                 VescSetPositionCmdPacket(vesc_id_, position).GetCanFrame(),
                 "SetPosition");
}

TransportStatus VescCanInterface::SetServo(double servo) {
  return SendCmd(can_, vesc_id_,
                 VescSetServoPosCmdPacket(vesc_id_, servo).GetCanFrame(),
                 "SetServo");
}
}  // namespace xmotion

/*
 * vesc_cmd_packet.cpp
 *
 * Created on 7/1/22 9:04 PM
 * Description:
 *
 * Copyright (c) 2022 Ruixiang Du (rdu)
 */

#include "motor_vesc/vesc_cmd_packet.hpp"

#include "xmsigma/serialization/byte_order.hpp"

namespace xmotion {
namespace {
using serialization::store_be16;
using serialization::store_be32;

void ClampCommand(float &cmd, float min, float max) {
  if (cmd > max) cmd = max;
  if (cmd < min) cmd = min;
}
}  // namespace

VescSetServoPosCmdPacket::VescSetServoPosCmdPacket(uint8_t vesc_id, float pos) {
  // limit command
  ClampCommand(pos, 0.0f, 1.0f);

  // convert to can frame
  frame_.id = VescFrame::VescProcessShortBufferCmdFrameId | vesc_id;
  frame_.extended = true;
  frame_.dlc = 5;
  frame_.data[0] = vesc_id;
  frame_.data[1] = 0x00;  // command type: send
  frame_.data[2] = VescFrame::VescCommSetServoPosId;

  store_be16(&frame_.data[3],
             static_cast<uint16_t>(static_cast<int16_t>(pos * 1000)));
}

VescSetDutyCycleCmdPacket::VescSetDutyCycleCmdPacket(uint8_t vesc_id,
                                                     float duty) {
  // limit command
  ClampCommand(duty, 0.0f, 1.0f);

  // convert to can frame
  frame_.id = VescFrame::VescDutyCycleCmdFrameId | vesc_id;
  frame_.extended = true;
  frame_.dlc = 4;
  store_be32(&frame_.data[0],
             static_cast<uint32_t>(static_cast<int32_t>(duty * 100000.0f)));
}

VescSetCurrentCmdPacket::VescSetCurrentCmdPacket(uint8_t vesc_id,
                                                 float current) {
  // Motor current is SIGNED: negative is regen/braking. The old [0, 20] clamp
  // silently floored regen to 0 (contradicting the driver's contract) — use a
  // symmetric magnitude bound so negative current reaches the wire. The app
  // envelope is enforced upstream in VescMotor; this is the packet backstop.
  ClampCommand(current, -20.0f, 20.0f);

  // convert to can frame
  frame_.id = VescFrame::VescCurrentCmdFrameId | vesc_id;
  frame_.extended = true;
  frame_.dlc = 4;
  store_be32(&frame_.data[0],
             static_cast<uint32_t>(static_cast<int32_t>(current * 1000.0f)));
}

VescSetCurrentBrakeCmdPacket::VescSetCurrentBrakeCmdPacket(uint8_t vesc_id,
                                                           float current) {
  // limit command
  ClampCommand(current, 0.0f, 20.0f);

  // convert to can frame
  frame_.id = VescFrame::VescCurrentBrakeCmdFrameId | vesc_id;
  frame_.extended = true;
  frame_.dlc = 4;
  store_be32(&frame_.data[0],
             static_cast<uint32_t>(static_cast<int32_t>(current * 1000.0f)));
}

VescSetRpmCmdPacket::VescSetRpmCmdPacket(uint8_t vesc_id, int32_t rpm) {
  // convert to can frame
  frame_.id = VescFrame::VescRpmCmdFrameId | vesc_id;
  frame_.extended = true;
  frame_.dlc = 4;
  store_be32(&frame_.data[0], static_cast<uint32_t>(rpm));
}

VescSetPositionCmdPacket::VescSetPositionCmdPacket(uint8_t vesc_id, float pos) {
  // limit command
  ClampCommand(pos, 0.0f, 1.0f);

  // convert to can frame
  frame_.id = VescFrame::VescPositionCmdFrameId | vesc_id;
  frame_.extended = true;
  frame_.dlc = 4;
  store_be32(&frame_.data[0],
             static_cast<uint32_t>(static_cast<int32_t>(pos * 100000.0f)));
}
}  // namespace xmotion

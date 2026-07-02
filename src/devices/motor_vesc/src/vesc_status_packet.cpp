/*
 * vesc_status_packet.cpp
 *
 * Created on 6/30/22 10:51 PM
 * Description:
 *
 * Reference:
 * [1] https://dongilc.gitbook.io/openrobot-inc/tutorials/control-with-can
 * [2] comm/comm_can.c in https://github.com/vedderb/bldc.git
 *
 * Copyright (c) 2022 Ruixiang Du (rdu)
 */

#include "motor_vesc/vesc_status_packet.hpp"

#include "xmsigma/serialization/byte_order.hpp"

namespace xmotion {
namespace {
using serialization::load_be16s;  // signed big-endian int16
using serialization::load_be32s;  // signed big-endian int32
}  // namespace

VescStatus1Packet::VescStatus1Packet(const CanFrame &frame)
    : VescFrame(frame) {
  // rpm(4 byte), current*10.0(2 byte), duty*1000.0(2 byte)
  rpm_ = load_be32s(&frame.data[0]);
  current_ = load_be16s(&frame.data[4]) / 10.0f;
  // duty is a SIGNED int16 (negative when reversing) — reading it unsigned
  // reported e.g. +65.036 instead of -0.5.
  duty_ = load_be16s(&frame.data[6]) / 1000.0f;
}

VescStatus2Packet::VescStatus2Packet(const CanFrame &frame)
    : VescFrame(frame) {
  //  amp_hours*10000.0(4 byte), amp_hours_charged*10000.0(4 byte)
  amp_hours_ = load_be32s(&frame.data[0]) / 10000.0f;
  amp_hours_charged_ = load_be32s(&frame.data[4]) / 10000.0f;
}

VescStatus3Packet::VescStatus3Packet(const CanFrame &frame)
    : VescFrame(frame) {
  // watt_hours*10000.0(4 byte), watt_hours_charged*10000.0(4 byte)
  watt_hours_ = load_be32s(&frame.data[0]) / 10000.0f;
  watt_hours_charged_ = load_be32s(&frame.data[4]) / 10000.0f;
}

VescStatus4Packet::VescStatus4Packet(const CanFrame &frame)
    : VescFrame(frame) {
  // temp_fet*10.0(2 byte), temp_motor*10.0(2 byte), current_in*10.0(2 byte),
  // pid_pos_now*50.0(2 byte)
  temp_fet_ = load_be16s(&frame.data[0]) / 10.0f;
  temp_motor_ = load_be16s(&frame.data[2]) / 10.0f;
  current_in_ = load_be16s(&frame.data[4]) / 10.0f;
  pid_pos_now_ = load_be16s(&frame.data[6]) / 50.0f;
}

VescStatus5Packet::VescStatus5Packet(const CanFrame &frame)
    : VescFrame(frame) {
  // tacho_value(4 byte), v_in*10.0(2 byte), reserved as 0(2 byte)
  tacho_value_ = load_be32s(&frame.data[0]);
  voltage_in_ = load_be16s(&frame.data[4]) / 10.0f;
}

VescStatus6Packet::VescStatus6Packet(const CanFrame &frame)
    : VescFrame(frame) {}
}  // namespace xmotion

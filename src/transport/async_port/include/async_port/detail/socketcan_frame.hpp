/*
 * socketcan_frame.hpp
 *
 * Internal conversion between the first-party CanFrame and Linux SocketCAN's
 * `struct can_frame`. Kept in a detail header (namespace xmotion::async_port_detail)
 * so the pure flag/id/dlc logic can be unit-tested without opening a PF_CAN
 * socket. This header pulls in <linux/can.h>; it is an implementation detail of
 * async_can.cpp and is NOT part of the public transport API (which speaks only
 * the first-party CanFrame, ADR 0003).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#ifndef ASYNC_PORT_DETAIL_SOCKETCAN_FRAME_HPP
#define ASYNC_PORT_DETAIL_SOCKETCAN_FRAME_HPP

#include <cstdint>
#include <cstring>

#include <linux/can.h>

#include "xmdriver/transport/can_frame.hpp"

namespace xmotion {
namespace async_port_detail {

// Convert a wire-level Linux frame to the first-party CanFrame. The EFF/RTR
// flags are pulled out of can_id, the id is masked to the relevant width, and
// the reported dlc is clamped to 8 (bytes past dlc are ignored).
inline CanFrame ToCanFrame(const struct can_frame &f) {
  CanFrame out;
  out.extended = (f.can_id & CAN_EFF_FLAG) != 0;
  out.remote = (f.can_id & CAN_RTR_FLAG) != 0;
  out.id = f.can_id & (out.extended ? CAN_EFF_MASK : CAN_SFF_MASK);
  out.dlc = f.can_dlc > 8 ? 8 : f.can_dlc;
  for (std::uint8_t i = 0; i < out.dlc; ++i) out.data[i] = f.data[i];
  return out;
}

// Convert a first-party CanFrame to the wire-level Linux frame. The id is masked
// to the relevant width, EFF/RTR flags are OR'd into can_id, and dlc is clamped
// to 8 (bytes past dlc are left zero).
inline struct can_frame ToLinuxFrame(const CanFrame &f) {
  struct can_frame out;
  memset(&out, 0, sizeof(out));
  out.can_id = f.id & (f.extended ? CAN_EFF_MASK : CAN_SFF_MASK);
  if (f.extended) out.can_id |= CAN_EFF_FLAG;
  if (f.remote) out.can_id |= CAN_RTR_FLAG;
  out.can_dlc = f.dlc > 8 ? 8 : f.dlc;
  for (std::uint8_t i = 0; i < out.can_dlc; ++i) out.data[i] = f.data[i];
  return out;
}

}  // namespace async_port_detail
}  // namespace xmotion

#endif /* ASYNC_PORT_DETAIL_SOCKETCAN_FRAME_HPP */

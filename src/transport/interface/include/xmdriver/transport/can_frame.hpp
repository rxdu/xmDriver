/*
 * can_frame.hpp
 *
 * First-party CAN frame at the transport boundary. The interface no longer
 * exposes linux/can.h's `struct can_frame` to consumers (ADR 0003), so the HAL
 * and drivers are not tied to a Linux-only type. The SocketCAN implementation
 * converts to/from `struct can_frame` internally.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <array>
#include <cstdint>

namespace xmotion {

struct CanFrame {
  std::uint32_t id = 0;              // 11-bit (standard) or 29-bit (extended)
  bool extended = false;            // extended (29-bit) identifier
  bool remote = false;              // remote-transmission-request frame
  std::uint8_t dlc = 0;             // data length, 0..8
  std::array<std::uint8_t, 8> data{};
};

}  // namespace xmotion

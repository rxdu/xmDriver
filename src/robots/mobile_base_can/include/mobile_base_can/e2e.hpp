/*
 * e2e.hpp — end-to-end protection (spec §4): CRC-8/AUTOSAR + alive counter.
 *
 * Every frame carries a uniform trailer: byte 6 = alive counter, byte 7 = CRC.
 * The CRC is computed over the frame's CAN id (Data-ID, for masquerade
 * protection) followed by data bytes 0..6, so a receiver can verify integrity
 * before dispatching on frame type.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */

#ifndef XMOTION_MOBILE_BASE_E2E_HPP
#define XMOTION_MOBILE_BASE_E2E_HPP

#include <cstddef>
#include <cstdint>

#include "xmdriver/transport/can_frame.hpp"

namespace xmotion::mobile_base {

// Trailer byte offsets, shared by every frame.
inline constexpr int kCounterByte = 6;
inline constexpr int kCrcByte = 7;

// CRC-8/AUTOSAR: poly 0x2F, init 0xFF, RefIn/RefOut false, XorOut 0xFF.
// Check value for the ASCII string "123456789" is 0xDF.
inline std::uint8_t Crc8Autosar(const std::uint8_t* data, std::size_t len) {
  std::uint8_t crc = 0xFF;
  for (std::size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80u) ? static_cast<std::uint8_t>((crc << 1) ^ 0x2Fu)
                          : static_cast<std::uint8_t>(crc << 1);
    }
  }
  return static_cast<std::uint8_t>(crc ^ 0xFFu);
}

// CRC over [id_lo, id_hi, data0..data6] — the Data-ID plus payload+counter.
inline std::uint8_t ComputeCrc(const CanFrame& f) {
  std::uint8_t buf[9];
  buf[0] = static_cast<std::uint8_t>(f.id & 0xFFu);
  buf[1] = static_cast<std::uint8_t>((f.id >> 8) & 0xFFu);
  for (int i = 0; i < kCounterByte + 1; ++i) buf[2 + i] = f.data[i];
  return Crc8Autosar(buf, sizeof(buf));
}

// Stamp the E2E trailer: caller supplies the per-id alive counter.
inline void StampE2e(CanFrame& f, std::uint8_t counter) {
  f.data[kCounterByte] = counter;
  f.data[kCrcByte] = ComputeCrc(f);
}

inline bool CrcValid(const CanFrame& f) {
  return f.data[kCrcByte] == ComputeCrc(f);
}

// Per-id alive-counter validator (spec §4). Construct one per frame id being
// consumed; feed it the received counter each frame.
class CounterCheck {
 public:
  enum class Result {
    kInit,      // first frame after boot/reset — accepted, reference set
    kInOrder,   // advanced by 1..max_delta — accepted
    kStuck,     // no advance (duplicate / replay) — reject
    kGap,       // advanced by > max_delta (too many losses) — reject, resync
  };

  explicit CounterCheck(std::uint8_t max_delta = 15) : max_delta_(max_delta) {}

  Result Update(std::uint8_t counter) {
    if (!have_) {
      have_ = true;
      last_ = counter;
      return Result::kInit;
    }
    const std::uint8_t delta = static_cast<std::uint8_t>(counter - last_);
    if (delta == 0) return Result::kStuck;          // do not advance on replay
    if (delta > max_delta_) {
      last_ = counter;                              // resync so we recover
      return Result::kGap;
    }
    last_ = counter;
    return Result::kInOrder;
  }

  // A result that means the frame is trustworthy for control.
  static bool Accepted(Result r) {
    return r == Result::kInit || r == Result::kInOrder;
  }

  void Reset() { have_ = false; }

 private:
  std::uint8_t max_delta_;
  bool have_ = false;
  std::uint8_t last_ = 0;
};

}  // namespace xmotion::mobile_base

#endif  // XMOTION_MOBILE_BASE_E2E_HPP

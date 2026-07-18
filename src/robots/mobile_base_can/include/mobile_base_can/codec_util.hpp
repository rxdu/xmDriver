/*
 * codec_util.hpp — little-endian pack/unpack + saturating scale helpers.
 *
 * PUBLIC on purpose: this is the extension mechanism. A per-platform model
 * profile builds its own frames with these same helpers (PutI16/GetI16/… plus
 * MakeFrame + StampE2e), so extension frames are byte-compatible with the core
 * and carry the same E2E trailer.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */

#ifndef XMOTION_MOBILE_BASE_CODEC_UTIL_HPP
#define XMOTION_MOBILE_BASE_CODEC_UTIL_HPP

#include <cmath>
#include <cstdint>

#include "mobile_base_can/e2e.hpp"
#include "xmdriver/transport/can_frame.hpp"

namespace xmotion::mobile_base {

// Saturating conversions. A non-finite source encodes as 0 (spec §5): a bad
// command must never become motion.
inline std::int16_t SatI16(double v) {
  if (std::isnan(v)) return 0;
  const double r = std::nearbyint(v);
  if (r < -32768.0) return -32768;
  if (r > 32767.0) return 32767;
  return static_cast<std::int16_t>(r);
}
inline std::uint16_t SatU16(double v) {
  if (std::isnan(v)) return 0;
  const double r = std::nearbyint(v);
  if (r < 0.0) return 0;
  if (r > 65535.0) return 65535;
  return static_cast<std::uint16_t>(r);
}

inline void PutI16(CanFrame& f, int off, std::int16_t v) {
  const auto u = static_cast<std::uint16_t>(v);
  f.data[off] = static_cast<std::uint8_t>(u & 0xFFu);
  f.data[off + 1] = static_cast<std::uint8_t>((u >> 8) & 0xFFu);
}
inline void PutU16(CanFrame& f, int off, std::uint16_t v) {
  f.data[off] = static_cast<std::uint8_t>(v & 0xFFu);
  f.data[off + 1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
}
inline void PutU32(CanFrame& f, int off, std::uint32_t v) {
  for (int i = 0; i < 4; ++i)
    f.data[off + i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu);
}

inline std::int16_t GetI16(const CanFrame& f, int off) {
  return static_cast<std::int16_t>(
      f.data[off] | (static_cast<std::uint16_t>(f.data[off + 1]) << 8));
}
inline std::uint16_t GetU16(const CanFrame& f, int off) {
  return static_cast<std::uint16_t>(
      f.data[off] | (static_cast<std::uint16_t>(f.data[off + 1]) << 8));
}
inline std::uint32_t GetU32(const CanFrame& f, int off) {
  std::uint32_t v = 0;
  for (int i = 0; i < 4; ++i)
    v |= static_cast<std::uint32_t>(f.data[off + i]) << (8 * i);
  return v;
}

// A well-formed, zeroed frame for `id` (DLC 8). Pack payload into bytes 0..5,
// then StampE2e() to fill the counter/CRC trailer.
inline CanFrame MakeFrame(std::uint32_t id) {
  CanFrame f;
  f.id = id;
  f.extended = false;
  f.remote = false;
  f.dlc = 8;
  f.data.fill(0);
  return f;
}

// Shape + integrity gate for every decoder (core and extension): exact id, a
// classic 8-byte data frame, and a valid E2E CRC (spec §4). Counter ordering is
// validated separately by the consumer (CounterCheck).
inline bool DecodableAs(const CanFrame& f, std::uint32_t id) {
  return f.id == id && !f.extended && !f.remote && f.dlc == 8 && CrcValid(f);
}

}  // namespace xmotion::mobile_base

#endif  // XMOTION_MOBILE_BASE_CODEC_UTIL_HPP

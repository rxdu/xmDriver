/*
 * id_list.hpp
 *
 * Shared helper for the Waveshare shared-bus array drivers: no-throw parsing
 * of the factory's params["ids"] comma-separated id list ("1,2,3,4"). Kept in
 * details/ — not part of the public driver API.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#ifndef XMMU_MOTOR_WAVESHARE_DETAILS_ID_LIST_HPP
#define XMMU_MOTOR_WAVESHARE_DETAILS_ID_LIST_HPP

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace xmotion::details {

// Parses "1,2,3,4" (spaces around commas allowed) into a byte id list.
// Returns an EMPTY vector on any malformed token (non-numeric, out of 0..255,
// trailing comma) so the caller can fail loudly instead of half-configuring.
// Never throws (factory creators must not leak exceptions across the HAL
// boundary).
inline std::vector<std::uint8_t> ParseIdList(const std::string& text) {
  std::vector<std::uint8_t> ids;
  const char* p = text.c_str();
  while (*p != '\0') {
    char* end = nullptr;
    const long v = std::strtol(p, &end, 10);
    if (end == p || v < 0 || v > 0xff) return {};
    ids.push_back(static_cast<std::uint8_t>(v));
    p = end;
    while (*p == ' ') ++p;
    if (*p == ',') {
      ++p;
      while (*p == ' ') ++p;
      if (*p == '\0') return {};  // trailing comma
    } else if (*p != '\0') {
      return {};
    }
  }
  return ids;
}

}  // namespace xmotion::details

#endif  // XMMU_MOTOR_WAVESHARE_DETAILS_ID_LIST_HPP

/*
 * transport_status.hpp
 *
 * Transport-level outcome for a send/queue operation. The transport layer is a
 * peer of the HAL and must not depend on it (ADR 0001: transport -/-> hal), so
 * it has its own small status vocabulary; drivers map it onto hal::Status.
 * Replaces the old void-returning sends that gave the caller no backpressure or
 * failure signal (ADR 0003).
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

namespace xmotion {

enum class TransportStatus {
  kOk = 0,       // accepted (sent or queued for send)
  kNotOpen,      // port not open
  kQueueFull,    // bounded TX queue is full (backpressure) — frame dropped
  kIoError,      // transport-level failure
  kInvalidArgument,  // malformed frame (bad dlc, oversized payload)
};

inline const char* ToString(TransportStatus s) {
  switch (s) {
    case TransportStatus::kOk: return "ok";
    case TransportStatus::kNotOpen: return "not_open";
    case TransportStatus::kQueueFull: return "queue_full";
    case TransportStatus::kIoError: return "io_error";
    case TransportStatus::kInvalidArgument: return "invalid_argument";
  }
  return "unknown";
}

}  // namespace xmotion

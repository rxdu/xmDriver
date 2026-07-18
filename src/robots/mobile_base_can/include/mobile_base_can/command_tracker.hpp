/*
 * command_tracker.hpp — base-side command gate (spec §4, §6, §9).
 *
 * The base feeds every received frame to OnFrame(); each control cycle it calls
 * Snapshot() for the vetted command state. The tracker enforces three gates so
 * the control loop never has to:
 *   - integrity: Decode checks CRC + shape; CounterCheck rejects replay/gap.
 *   - version:   a HEARTBEAT whose protocol version differs is not trusted.
 *   - freshness: a TWIST older than command_timeout is treated as absent (the
 *                snapshot reports zero velocity — stale commands never drive).
 * It also latches the commanded e-stop (engage sticks; clear is key-validated by
 * the decoder). It is model-agnostic: it consumes only the four core command ids
 * and ignores everything else (state frames, model-profile frames).
 *
 * Time is caller-supplied (a steady monotonic point), so the tracker is fully
 * testable without a real clock and imposes no threading policy.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu). SPDX-License-Identifier: Apache-2.0
 */

#ifndef XMOTION_MOBILE_BASE_COMMAND_TRACKER_HPP
#define XMOTION_MOBILE_BASE_COMMAND_TRACKER_HPP

#include <chrono>
#include <cstdint>

#include "mobile_base_can/core_codec.hpp"

namespace xmotion::mobile_base {

struct TrackerConfig {
  // A TWIST older than this is stale; Snapshot then reports zero velocity.
  std::chrono::milliseconds command_timeout{200};
  // Protocol version the base speaks; a HEARTBEAT that differs is untrusted.
  std::uint8_t expected_version = kProtocolVersion;
  // Max forward jump tolerated in an alive counter before it reads as a gap.
  std::uint8_t max_counter_delta = 15;
};

// Vetted command state for one control cycle. Velocities are already gated: if
// they are not safe to act on, they are zero and twist_fresh is false.
struct CommandSnapshot {
  double vx = 0.0, vy = 0.0, wz = 0.0;  // last accepted twist, or 0 if not fresh
  bool twist_fresh = false;             // recent + version-ok + integrity-ok
  ModeRequest mode = ModeRequest::kStandby;  // last accepted mode request
  bool estop_engaged = false;           // latched from ESTOP engage/clear
  bool version_ok = true;               // last HEARTBEAT matched expected_version
  std::uint32_t faults = 0;             // sticky fault word (spec §10 bits)
};

class CommandTracker {
 public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;

  explicit CommandTracker(TrackerConfig cfg = {})
      : cfg_(cfg),
        twist_ctr_(cfg.max_counter_delta),
        mode_ctr_(cfg.max_counter_delta),
        estop_ctr_(cfg.max_counter_delta),
        hb_ctr_(cfg.max_counter_delta) {}

  // Feed one received frame. Returns true iff it was an accepted core command
  // (passed integrity, and for HEARTBEAT recorded a version result). Non-command
  // ids (state / model-profile frames) are ignored and return false.
  bool OnFrame(const CanFrame& f, TimePoint now) {
    switch (f.id) {
      case kCmdTwist: {
        const auto c = DecodeTwist(f);
        if (!Accept(c, twist_ctr_)) return false;
        vx_ = c->vx;
        vy_ = c->vy;
        wz_ = c->wz;
        last_twist_ = now;
        have_twist_ = true;
        return true;
      }
      case kCmdMode: {
        const auto c = DecodeMode(f);
        if (!Accept(c, mode_ctr_)) return false;
        mode_ = c->mode;
        return true;
      }
      case kCmdEStop: {
        const auto c = DecodeEStop(f);  // clear-key already validated in decode
        if (!Accept(c, estop_ctr_)) return false;
        estop_engaged_ = (c->action == EStopAction::kEngage);
        return true;
      }
      case kCmdHeartbeat: {
        const auto c = DecodeHeartbeat(f);
        if (!Accept(c, hb_ctr_)) return false;
        version_ok_ = (c->version == cfg_.expected_version);
        if (!version_ok_) faults_ |= kFaultProtoVersion;
        return true;
      }
      default:
        return false;  // not a core command — not this tracker's concern
    }
  }

  CommandSnapshot Snapshot(TimePoint now) const {
    CommandSnapshot s;
    s.mode = mode_;
    s.estop_engaged = estop_engaged_;
    s.version_ok = version_ok_;
    s.faults = faults_;
    const bool recent =
        have_twist_ && (now - last_twist_) <= cfg_.command_timeout;
    s.twist_fresh = recent && version_ok_;
    if (s.twist_fresh) {
      s.vx = vx_;
      s.vy = vy_;
      s.wz = wz_;
    }  // else velocities stay 0 — a stale or untrusted command never drives.
    return s;
  }

  std::uint32_t faults() const { return faults_; }
  void ClearFaults() { faults_ = 0; }

 private:
  // Integrity gate shared by every command id: reject on failed CRC/shape
  // (decode == nullopt) or a replay/gap counter, latching kFaultCmdIntegrity.
  template <typename Opt>
  bool Accept(const Opt& decoded, CounterCheck& ctr) {
    if (!decoded) {
      faults_ |= kFaultCmdIntegrity;
      return false;
    }
    if (!CounterCheck::Accepted(ctr.Update(decoded->counter))) {
      faults_ |= kFaultCmdIntegrity;
      return false;
    }
    return true;
  }

  TrackerConfig cfg_;
  double vx_ = 0.0, vy_ = 0.0, wz_ = 0.0;
  TimePoint last_twist_{};
  bool have_twist_ = false;
  ModeRequest mode_ = ModeRequest::kStandby;
  bool estop_engaged_ = false;
  bool version_ok_ = true;  // optimistic until a HEARTBEAT says otherwise
  std::uint32_t faults_ = 0;
  CounterCheck twist_ctr_, mode_ctr_, estop_ctr_, hb_ctr_;
};

}  // namespace xmotion::mobile_base

#endif  // XMOTION_MOBILE_BASE_COMMAND_TRACKER_HPP

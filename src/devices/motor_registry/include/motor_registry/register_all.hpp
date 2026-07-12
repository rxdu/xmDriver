/*
 * register_all.hpp
 *
 * One-call registration of every bundled motor driver with hal::MotorFactory.
 *
 * Config-driven construction (MotorFactory) needs each driver's creator to be
 * registered. Drivers self-register via a static initializer, but a static
 * library's initializer is dropped by the linker unless the TU is referenced —
 * so relying on it alone is fragile. Calling RegisterAllMotors() once at startup
 * forces every driver TU to be linked and registered, regardless of linker
 * garbage collection (ADR 0003). Apps that want only a subset can instead call
 * the individual RegisterXxx() functions.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

namespace xmotion {

// Registers all bundled motor drivers ("vesc", "akelc", "ddsm210",
// "ddsm210_array", "sms_sts", "sms_sts_array") plus the hardware-free
// reference "sim" motor. Idempotent. Returns true.
bool RegisterAllMotors();

}  // namespace xmotion

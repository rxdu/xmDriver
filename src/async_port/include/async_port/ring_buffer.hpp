/*
 * ring_buffer.hpp
 *
 * Compatibility facade. RingBuffer was promoted to the xmSigma foundation
 * (xmsigma/container/ring_buffer.hpp) after it had been duplicated and drifted
 * across components — see xmSigma ADR 0001. This header re-exports the canonical
 * type so existing includes (async_port/ring_buffer.hpp -> xmotion::RingBuffer)
 * keep working; new code should include xmsigma/container/ring_buffer.hpp
 * directly.
 *
 * Copyright (c) 2019-2026 Ruixiang Du (rdu)
 */

#pragma once

#include "xmsigma/container/ring_buffer.hpp"

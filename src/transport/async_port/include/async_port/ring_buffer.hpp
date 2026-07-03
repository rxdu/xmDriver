/*
 * ring_buffer.hpp
 *
 * Compatibility facade. RingBuffer was promoted to the xmBase foundation
 * (xmbase/container/ring_buffer.hpp) after it had been duplicated and drifted
 * across components — see xmBase ADR 0001. This header re-exports the canonical
 * type so existing includes (async_port/ring_buffer.hpp -> xmotion::RingBuffer)
 * keep working; new code should include xmbase/container/ring_buffer.hpp
 * directly.
 *
 * Copyright (c) 2019-2026 Ruixiang Du (rdu)
 */

#pragma once

#include "xmbase/container/ring_buffer.hpp"

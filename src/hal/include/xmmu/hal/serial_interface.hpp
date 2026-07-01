/*
 * serial_interface.hpp
 *
 * Compatibility facade. The transport interfaces were moved below the device HAL
 * into xmmu/transport/ (see docs/adr/0001). This header re-exports the new
 * location so existing includes keep working; new code should include
 * "xmmu/transport/serial_interface.hpp" directly.
 *
 * Copyright (c) 2023-2026 Ruixiang Du (rdu)
 */

#pragma once

#include "xmmu/transport/serial_interface.hpp"

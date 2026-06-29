/*
 * modbus_rtu_client.hpp
 *
 * Created on 4/21/24 12:13 AM
 * Description:
 *
 * Copyright (c) 2024 Ruixiang Du (rdu)
 */

#pragma once

#include <cstdint>
#include <memory>
#include "xmmu/hal/modbus_rtu_interface.hpp"

namespace xmotion {
class ModbusRtuClient {
 public:
  ModbusRtuClient() = default;

  ModbusRtuClient(std::shared_ptr<ModbusRtuInterface> port, int device_id)
      : port_(port), device_id_(device_id) {}

  virtual ~ModbusRtuClient() = default;

  // public interface
  virtual bool IsReachable() = 0;

 protected:
  std::shared_ptr<ModbusRtuInterface> port_;
  int device_id_;
};
}  // namespace xmotion


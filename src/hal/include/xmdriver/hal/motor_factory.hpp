/*
 * motor_factory.hpp
 *
 * The construction seam. Upper-layer code builds motors from a config value, not
 * by naming a concrete driver type — so swapping hardware (VESC -> AKELC -> sim)
 * is a config change, with no app edit and no driver header included by the app.
 *
 * Drivers self-register a creator under a type name; apps call Create().
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "xmdriver/hal/motor_controller.hpp"

namespace xmotion::hal {

// Generic, transport-agnostic motor description. `type` selects the driver;
// `bus`/`id` locate it; the limit fields define the safe envelope the driver
// must clamp to. `params` carries any driver-specific extras without leaking
// them into this struct.
struct MotorConfig {
  std::string type;             // "vesc", "akelc", "ddsm210", "sim", ...
  std::string bus;              // "can0", "/dev/ttyUSB0", ...
  int id = 0;                   // node / slave id
  double max_speed_rpm = 0.0;   // 0 => unset; driver should still clamp sanely
  double max_current_a = 0.0;
  std::map<std::string, std::string> params;  // driver-specific extras
};

class MotorFactory {
 public:
  using Creator = std::function<std::unique_ptr<Motor>(const MotorConfig&)>;

  static MotorFactory& Instance() {
    static MotorFactory inst;
    return inst;
  }

  void Register(const std::string& type, Creator creator) {
    creators_[type] = std::move(creator);
  }

  bool IsRegistered(const std::string& type) const {
    return creators_.find(type) != creators_.end();
  }

  // Returns nullptr if `cfg.type` is unknown (caller checks) — construction
  // failure is not an exception here.
  std::unique_ptr<Motor> Create(const MotorConfig& cfg) const {
    auto it = creators_.find(cfg.type);
    if (it == creators_.end()) return nullptr;
    return it->second(cfg);
  }

 private:
  MotorFactory() = default;
  std::map<std::string, Creator> creators_;
};

// Helper for driver self-registration at static-init:
//   static const MotorRegistrar kReg{"vesc", [](const MotorConfig& c){...}};
struct MotorRegistrar {
  MotorRegistrar(const std::string& type, MotorFactory::Creator creator) {
    MotorFactory::Instance().Register(type, std::move(creator));
  }
};

}  // namespace xmotion::hal

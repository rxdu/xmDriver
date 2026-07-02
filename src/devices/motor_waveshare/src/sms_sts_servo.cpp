/*
 * sms_sts_servo.cpp
 *
 * @copyright Copyright (c) 2024 Ruixiang Du (rdu)
 */

#include "motor_waveshare/sms_sts_servo.hpp"

// Impl is defined inline here (not compiled as a standalone TU).
#include "sms_sts_servo_impl.cpp"

namespace xmotion {

SmsStsServo::SmsStsServo(Config cfg)
    : pimpl_(std::make_unique<Impl>(std::move(cfg))) {}

SmsStsServo::~SmsStsServo() = default;

hal::Status SmsStsServo::Connect() { return pimpl_->Connect(); }
void SmsStsServo::Disconnect() { pimpl_->Disconnect(); }
bool SmsStsServo::IsConnected() const { return pimpl_->IsConnected(); }
hal::DeviceHealth SmsStsServo::Health() const { return pimpl_->Health(); }

hal::Status SmsStsServo::Stop() { return pimpl_->Stop(); }

hal::Status SmsStsServo::SetPosition(hal::Radian position) {
  return pimpl_->SetPosition(position);
}
hal::Result<hal::Radian> SmsStsServo::GetPosition() {
  return pimpl_->GetPosition();
}

hal::Status SmsStsServo::SetSpeed(hal::Rpm speed) {
  return pimpl_->SetSpeed(speed);
}
hal::Result<hal::Rpm> SmsStsServo::GetSpeed() { return pimpl_->GetSpeed(); }

hal::Result<SmsStsServo::State> SmsStsServo::GetState() const {
  return pimpl_->GetState();
}

hal::Status SmsStsServo::SetMode(Mode mode) { return pimpl_->SetMode(mode); }
hal::Status SmsStsServo::SetMotorId(uint8_t id) {
  return pimpl_->SetMotorId(id);
}
hal::Status SmsStsServo::SetNeutralPosition() {
  return pimpl_->SetNeutralPosition();
}

bool RegisterSmsStsServo() {
  hal::MotorFactory::Instance().Register(
      "sms_sts", [](const hal::MotorConfig& c) -> std::unique_ptr<hal::Motor> {
        SmsStsServo::Config sc;
        sc.bus = c.bus;
        sc.id = static_cast<std::uint8_t>(c.id);
        sc.max_speed = c.max_speed_rpm;  // native step/sec envelope
        auto it = c.params.find("baud");
        if (it != c.params.end()) sc.baud = std::stoi(it->second);
        auto off = c.params.find("position_offset_deg");
        if (off != c.params.end()) sc.position_offset_deg = std::stod(off->second);
        return std::make_unique<SmsStsServo>(std::move(sc));
      });
  return true;
}

namespace {
const bool kSmsStsServoRegistered = RegisterSmsStsServo();
}  // namespace

}  // namespace xmotion

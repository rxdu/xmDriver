/*
 * vesc_can_interface.hpp
 *
 * Created on 6/30/22 10:21 PM
 * Description:
 *
 * Copyright (c) 2022 Ruixiang Du (rdu)
 */

#ifndef ROBOSW_SRC_DRIVER_INCLUDE_VESC_DRIVER_VESC_CAN_INTERFACE_HPP
#define ROBOSW_SRC_DRIVER_INCLUDE_VESC_DRIVER_VESC_CAN_INTERFACE_HPP

#include <mutex>
#include <memory>
#include <functional>

#include "xmmu/transport/can_interface.hpp"
#include "xmmu/transport/can_frame.hpp"
#include "xmmu/transport/transport_status.hpp"
#include "motor_vesc/vesc_state.hpp"

namespace xmotion {
class VescCanInterface {
 public:
  using StateUpdatedCallback = std::function<void(const StampedVescState &)>;
  // Fired on an asynchronous bus fault (bus-off, interface down, unplug) so an
  // owner can degrade/fault instead of only discovering a dead link on the next
  // failed send.
  using ErrorCallback = std::function<void(TransportStatus)>;

 public:
  VescCanInterface() = default;

  // Opens an AsyncCAN on `can` and wires it to this interface. Convenience path
  // for production; delegates to the transport-injecting overload below so all
  // callback wiring lives in one place.
  bool Connect(const std::string &can, uint8_t vesc_id);
  // Test / shared-bus seam: adopt an already-constructed transport instead of
  // creating an AsyncCAN. Installs the receive/error callbacks and Open()s it.
  // Returns false if `can` is null.
  bool Connect(std::shared_ptr<CanInterface> can, uint8_t vesc_id);
  void Disconnect();

  uint8_t GetVescId() const;

  void SetStateUpdatedCallback(StateUpdatedCallback cb);
  void SetErrorCallback(ErrorCallback cb);
  StampedVescState GetLastState() const;

  void RequestFwVersion();
  void RequestState();
  void RequestImuData();

  // Sends return the transport outcome; a failed send is logged and the status
  // is propagated so the caller can map it onto hal::Status (never swallowed).
  TransportStatus SetDutyCycle(double duty_cycle);
  TransportStatus SetCurrent(double current);
  TransportStatus SetBrake(double brake);
  TransportStatus SetSpeed(double speed);
  TransportStatus SetPosition(double position);
  TransportStatus SetServo(double servo);

 private:
  std::shared_ptr<CanInterface> can_;
  uint8_t vesc_id_ = 0;

  mutable std::mutex state_mtx_;
  StampedVescState stamped_state_;
  StateUpdatedCallback state_updated_callback_;
  ErrorCallback error_callback_;

  void HandleCanFrame(const CanFrame &frame);
};
}  // namespace xmotion

#endif  // ROBOSW_SRC_DRIVER_INCLUDE_VESC_DRIVER_VESC_CAN_INTERFACE_HPP

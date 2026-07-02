/*
 * ImuHipnuc.hpp
 *
 * Created on: Nov 22, 2021 22:11
 * Description:
 *
 * Copyright (c) 2021 Ruixiang Du (rdu)
 */

#ifndef IMU_HIPNUC_HPP
#define IMU_HIPNUC_HPP

#include <memory>
#include <mutex>

#include "xmmu/transport/serial_interface.hpp"
#include "xmmu/hal/imu_interface.hpp"

namespace xmotion {
class ImuHipnuc : public ImuInterface {
 public:
  ImuHipnuc();
  ~ImuHipnuc();

  // public API
  bool Connect(std::string uart_name, uint32_t baud_rate = 115200) override;
  void Disconnect() override;
  bool IsConnected() override;

  void GetLastImuData(ImuData *data) override;

 private:
  std::shared_ptr<SerialInterface> serial_;

  // Latest decoded sample (GetLastImuData copies this out); guarded because it
  // is written on the serial RX thread and read by callers.
  mutable std::mutex data_mtx_;
  ImuData last_data_;

  // Per-instance HiPNUC parser accumulator (hipnuc_raw_t*, opaque to avoid
  // leaking the vendored ch_serial.h into this public header). The old code used
  // a function-local `static`, shared across instances and never reset.
  void *raw_state_ = nullptr;

  bool Connect(std::string dev_name) { return false; };
  void ParseSerialData(uint8_t *data, const size_t bufsize, size_t len);
};
}  // namespace xmotion

#endif /* IMU_HIPNUC_HPP */

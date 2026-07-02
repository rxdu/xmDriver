/*
 * imu_hipnuc.cpp
 *
 * Created on: Nov 22, 2021 22:22
 * Description:
 *
 * Copyright (c) 2021 Ruixiang Du (rdu)
 */

#include "sensor_imu/imu_hipnuc.hpp"

#include <cmath>

#include "async_port/async_serial.hpp"

extern "C" {
#include "ch_serial.h"
}

namespace xmotion {
namespace {
static constexpr float g_const = 9.81;

void UnpackData(hipnuc_raw_t *data, ImuData *data_imu) {
  data_imu->id = 0x91;

  data_imu->timestamp = data->imu.ts;

  data_imu->pressure = data->imu.prs;

  data_imu->accel.x = -data->imu.acc[0] * g_const;
  data_imu->accel.y = -data->imu.acc[1] * g_const;
  data_imu->accel.z = -data->imu.acc[2] * g_const;

  data_imu->gyro.x = data->imu.gyr[0] * (M_PI / 180.0);
  data_imu->gyro.y = data->imu.gyr[1] * (M_PI / 180.0);
  data_imu->gyro.z = data->imu.gyr[2] * (M_PI / 180.0);

  data_imu->magn.x = data->imu.mag[0];
  data_imu->magn.y = data->imu.mag[1];
  data_imu->magn.z = data->imu.mag[2];

  data_imu->euler.roll = data->imu.eul[0];
  data_imu->euler.pitch = data->imu.eul[1];
  data_imu->euler.yaw = data->imu.eul[2];

  data_imu->quat.w() = data->imu.quat[0];
  data_imu->quat.x() = data->imu.quat[1];
  data_imu->quat.y() = data->imu.quat[2];
  data_imu->quat.z() = data->imu.quat[3];
}
}  // namespace

ImuHipnuc::ImuHipnuc() {
  last_data_.quat.setIdentity();  // Eigen leaves quaternions uninitialized
  raw_state_ = new hipnuc_raw_t{};
}

ImuHipnuc::~ImuHipnuc() {
  if (serial_) serial_->Close();
  delete static_cast<hipnuc_raw_t *>(raw_state_);
}

bool ImuHipnuc::Connect(std::string uart_name, uint32_t baud_rate) {
  if (!serial_) serial_ = std::make_shared<AsyncSerial>(uart_name, baud_rate);
  if (serial_->IsOpened()) serial_->Close();

  // Reset the parser accumulator so a reconnect never resumes mid-frame.
  *static_cast<hipnuc_raw_t *>(raw_state_) = hipnuc_raw_t{};

  serial_->SetReceiveCallback(
      std::bind(&ImuHipnuc::ParseSerialData, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3));

  return serial_->Open() && serial_->IsOpened();
}

void ImuHipnuc::Disconnect() {
  if (serial_) serial_->Close();
}

bool ImuHipnuc::IsConnected() { return serial_ && serial_->IsOpened(); }

void ImuHipnuc::GetLastImuData(ImuData *data) {
  if (data == nullptr) return;
  std::lock_guard<std::mutex> lock(data_mtx_);
  *data = last_data_;  // was an empty body -> caller got uninitialized garbage
}

void ImuHipnuc::ParseSerialData(uint8_t *data, const size_t bufsize,
                                size_t len) {
  auto *raw = static_cast<hipnuc_raw_t *>(raw_state_);
  for (size_t i = 0; i < len; ++i) {
    if (ch_serial_input(raw, data[i])) {
      ImuData imu;
      UnpackData(raw, &imu);  // populates all fields incl. the quaternion

      {
        std::lock_guard<std::mutex> lock(data_mtx_);
        last_data_ = imu;
      }
      if (callback_ != nullptr) callback_(imu);
    }
  }
}
}  // namespace xmotion
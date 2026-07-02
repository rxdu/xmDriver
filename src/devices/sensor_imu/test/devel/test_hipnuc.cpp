/*
 * test_hipnuc.cpp — manual hardware demo for the HiPNUC IMU on the new HAL.
 *
 * Requires a real HiPNUC IMU on the serial port below. Streams samples via the
 * push callback and prints the observed update rate.
 *
 * Copyright (c) 2021-2026 Ruixiang Du (rdu)
 */

#include <chrono>
#include <iostream>
#include <thread>

#include "sensor_imu/imu_hipnuc.hpp"

using namespace xmotion;

namespace {
using Clock = std::chrono::steady_clock;
Clock::time_point g_last_update;

void OnSample(const hal::ImuSample& s) {
  const auto now = Clock::now();
  const auto dt =
      std::chrono::duration_cast<std::chrono::microseconds>(now - g_last_update)
          .count();
  g_last_update = now;
  const double rate = dt > 0 ? 1e6 / static_cast<double>(dt) : 0.0;
  std::cout << "Accel: " << s.accel.x << " " << s.accel.y << " " << s.accel.z
            << " Gyro: " << s.gyro.x << " " << s.gyro.y << " " << s.gyro.z
            << ", Update rate: " << rate << " Hz" << std::endl;
}
}  // namespace

int main(int argc, char** argv) {
  ImuHipnuc::Config cfg;
  cfg.device = (argc > 1) ? argv[1] : "/dev/ttyUSB0";
  cfg.baud_rate = (argc > 2) ? static_cast<std::uint32_t>(std::stoul(argv[2]))
                             : 921600;

  ImuHipnuc imu(cfg);
  imu.SetSampleCallback(OnSample);
  if (imu.Connect() != hal::Status::kOk) {
    std::cerr << "Failed to open device " << cfg.device << std::endl;
    return -1;
  }

  while (imu.IsConnected()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return 0;
}

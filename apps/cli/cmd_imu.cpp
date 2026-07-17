/*
 * cmd_imu.cpp — HiPNUC IMU: connect and stream accel / gyro / orientation.
 *
 * Receive-only device; no command path. Streams at ~10 Hz until Ctrl-C.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include "cli_common.hpp"
#include "commands.hpp"
#include "sensor_imu/imu_hipnuc.hpp"

using namespace xmotion;

int xmcli::RunImu(const Args& args) {
  const std::string port = ResolvePort(args);
  if (port.empty()) return 2;

  ImuHipnuc::Config cfg;
  cfg.device = port;
  // The HiPNUC on this hardware streams at 921600 (matches the sensor_imu dev
  // tool default); the driver struct's generic 115200 yields no decoded frames.
  cfg.baud_rate = 921600;
  if (args.has("baud")) {
    int b = 0;
    if (ParseInt(args.get("baud"), b)) cfg.baud_rate = static_cast<std::uint32_t>(b);
  }
  ImuHipnuc imu(cfg);

  std::cout << "imu: connecting on " << port << " @ " << cfg.baud_rate
            << " baud (Ctrl-C to stop) ...\n";
  if (imu.Connect() != hal::Status::kOk) {
    std::cerr << "imu: FAILED to open " << port << "\n";
    return 1;
  }

  std::cout << "waiting for samples (no output => wiring / baud)\n";
  std::uint64_t n = 0;
  while (!g_stop.load()) {
    const auto r = imu.Read();
    if (r.ok()) {
      const auto& s = r.value;
      ++n;
      std::cout << "\racc[" << s.accel.x << ' ' << s.accel.y << ' ' << s.accel.z
                << "] m/s^2  gyr[" << s.gyro.x << ' ' << s.gyro.y << ' '
                << s.gyro.z << "] rad/s  quat[" << s.orientation.w() << ' '
                << s.orientation.x() << ' ' << s.orientation.y() << ' '
                << s.orientation.z() << "] (w,x,y,z)  temp=" << s.temperature
                << " degC    " << std::flush;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  imu.Disconnect();
  std::cout << "\nimu: stopped after " << n << " read(s)\n";
  return n > 0 ? 0 : 1;
}

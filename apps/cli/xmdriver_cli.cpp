/*
 * xmdriver_cli.cpp
 *
 * One binary, a subcommand per device, for bringing up xmDriver devices on real
 * hardware in isolation (no robot config). Interactive by default: run with no
 * device (or no --port) on a terminal and it prompts with the valid choices;
 * pass flags to skip the prompts and script it.
 *
 *   xmdriver_cli                       # menu: pick a device, then a port
 *   xmdriver_cli sbus  --port /dev/ttyACM0
 *   xmdriver_cli ddsm  --port /dev/ttyACM1 --id 1 [--spin 20 --confirm]
 *   xmdriver_cli servo --port /dev/ttyACM2 --id 1 [--move 30 --confirm]
 *   xmdriver_cli imu   --port /dev/ttyACM3
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "cli_common.hpp"
#include "commands.hpp"

namespace {
struct Device {
  const char* name;
  const char* desc;
  std::function<int(const xmcli::Args&)> run;
};
}  // namespace

int main(int argc, char** argv) {
  using namespace xmcli;

  const std::vector<Device> devices = {
      {"sbus", "SBUS RC receiver (serial 100000 8E2) — live channels", RunSbus},
      {"ddsm", "Waveshare DDSM-210 hub motor (serial) — feedback / spin",
       RunDdsm},
      {"servo", "Waveshare SMS/STS bus servo (serial) — feedback / move",
       RunServo},
      {"imu", "HiPNUC IMU (serial) — live accel / gyro / orientation", RunImu},
  };

  const Args args = ParseArgs(argc, argv);
  InstallSignalHandler();

  auto find = [&](const std::string& n) -> int {
    for (std::size_t i = 0; i < devices.size(); ++i)
      if (n == devices[i].name) return static_cast<int>(i);
    return -1;
  };

  int idx = args.device.empty() ? -1 : find(args.device);
  if (idx < 0) {
    if (!args.device.empty())
      std::cerr << "xmdriver_cli: unknown device '" << args.device << "'\n";
    if (!StdinIsTty()) {
      std::cerr << "usage: xmdriver_cli <device> [--port DEV] [--id N] "
                   "[--set-mode speed] [--spin RPM|--move DEG] [--confirm]\n"
                   "  devices:";
      for (const auto& d : devices) std::cerr << ' ' << d.name;
      std::cerr << "\n";
      return 2;
    }
    std::vector<std::string> menu;
    menu.reserve(devices.size());
    for (const auto& d : devices)
      menu.emplace_back(std::string(d.name) + "  —  " + d.desc);
    idx = PromptChoice("xmdriver_cli — select a device to test:", menu);
    if (idx < 0) return 1;
  }

  return devices[idx].run(args);
}

/*
 * cmd_sbus.cpp — SBUS RC receiver: connect and stream decoded channels.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

#include "cli_common.hpp"
#include "commands.hpp"
#include "input_sbus/sbus_receiver.hpp"

using namespace xmotion;

int xmcli::RunSbus(const Args& args) {
  const std::string port = ResolvePort(args);
  if (port.empty()) return 2;

  std::cout << "sbus: " << port << " @ 100000 baud, 8E2 (Ctrl-C to stop)\n";
  SbusReceiver rx(port);
  if (rx.Connect() != hal::Status::kOk) {
    std::cerr << "sbus: FAILED to connect on " << port
              << " (permission? wrong device? baud?)\n";
    return 1;
  }

  std::atomic<std::uint64_t> frames{0};
  rx.SetFrameCallback([&frames](const hal::RcFrame& f) {
    const std::uint64_t n = ++frames;
    std::cout << "\rframe " << n << (f.failsafe ? " FAILSAFE" : "")
              << (f.frame_loss ? " LOSS" : "") << "  ch:";
    for (int c = 0; c < 16; ++c)
      std::cout << ' ' << static_cast<int>(f.channels[c]);
    std::cout << "    " << std::flush;
  });

  std::cout << "waiting for frames (no output => check wiring / inversion / "
               "polarity)\n";
  while (!g_stop.load())
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

  rx.Disconnect();
  std::cout << "\nsbus: stopped after " << frames.load() << " frame(s)\n";
  return frames.load() > 0 ? 0 : 1;
}

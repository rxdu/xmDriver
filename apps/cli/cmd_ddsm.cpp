/*
 * cmd_ddsm.cpp — Waveshare DDSM-210 hub motor: read feedback; optional spin.
 *
 * Default is read-only (mode / speed / encoder). Motion (--spin RPM) is gated:
 * it requires the motor to already be in SPEED mode and an explicit --confirm
 * (or an interactive y/N), and it is time-boxed.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

#include "cli_common.hpp"
#include "commands.hpp"
#include "motor_waveshare/ddsm_210.hpp"

using namespace xmotion;

namespace {
const char* DdsmModeName(Ddsm210::Mode m) {
  switch (m) {
    case Ddsm210::Mode::kOpenLoop:
      return "open-loop";
    case Ddsm210::Mode::kSpeed:
      return "speed";
    case Ddsm210::Mode::kPosition:
      return "position";
    case Ddsm210::Mode::kUnknown:
      return "unknown";
  }
  return "?";
}

bool ParseDdsmMode(const std::string& s, Ddsm210::Mode& out) {
  if (s == "speed") {
    out = Ddsm210::Mode::kSpeed;
  } else if (s == "position") {
    out = Ddsm210::Mode::kPosition;
  } else if (s == "open-loop" || s == "openloop" || s == "open") {
    out = Ddsm210::Mode::kOpenLoop;
  } else {
    return false;
  }
  return true;
}

void DdsmSpin(Ddsm210& motor, double rpm, double secs) {
  // Motion needs speed mode; the switch is a cheap runtime command (the motor
  // may boot into any default), so ensure it here rather than assuming.
  if (motor.GetMode() != Ddsm210::Mode::kSpeed) {
    std::cout << "  switching to speed mode ...\n";
    if (motor.SetMode(Ddsm210::Mode::kSpeed) != hal::Status::kOk) {
      std::cout << "  could not switch to speed mode; not spinning\n";
      return;
    }
  }
  std::cout << "  spinning " << rpm << " rpm for " << secs
            << " s (Ctrl-C aborts) ...\n";
  const auto t_end = std::chrono::steady_clock::now() +
                     std::chrono::milliseconds(static_cast<int>(secs * 1000.0));
  while (std::chrono::steady_clock::now() < t_end && !xmcli::g_stop.load()) {
    motor.SetSpeed(hal::Rpm{rpm});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto s = motor.GetSpeed();
    if (s.ok())
      std::cout << "\r  measured " << s.value.value() << " rpm    "
                << std::flush;
  }
  motor.Stop();
  std::cout << "\n  stopped\n";
}

// Interactive console: refresh feedback and pick switch-mode / spin / stop from
// a menu. Runs until the user quits or Ctrl-C. Returns a process exit code.
int DdsmInteractive(Ddsm210& motor, int id) {
  while (!xmcli::g_stop.load()) {
    motor.RequestModeFeedback();
    motor.RequestOdometryFeedback();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    const Ddsm210::Mode mode = motor.GetMode();
    const auto spd = motor.GetSpeed();
    std::cout << "\n[ddsm id " << id << "]  mode=" << DdsmModeName(mode)
              << "  speed="
              << (spd.ok() ? std::to_string(spd.value.value())
                           : std::string("stale"))
              << " rpm  encoder=" << motor.GetEncoderCount() << "\n";

    const int choice = xmcli::PromptChoice(
        "action:", {"refresh feedback", "switch mode", "spin (time-boxed)",
                    "stop", "quit"});
    if (choice < 0 || choice == 4) break;  // EOF or quit

    switch (choice) {
      case 0:
        break;  // loop refreshes feedback
      case 1: {
        const int m =
            xmcli::PromptChoice("switch to:", {"speed", "position", "open-loop"});
        if (m < 0) break;
        const Ddsm210::Mode target = (m == 0)   ? Ddsm210::Mode::kSpeed
                                     : (m == 1) ? Ddsm210::Mode::kPosition
                                                : Ddsm210::Mode::kOpenLoop;
        std::cout << "  switching to " << DdsmModeName(target) << " ...\n";
        if (motor.SetMode(target) != hal::Status::kOk)
          std::cout << "  mode switch NOT confirmed (id / wiring?)\n";
        else
          std::cout << "  mode now " << DdsmModeName(motor.GetMode()) << "\n";
        break;
      }
      case 2: {
        const int rpm = xmcli::PromptInt("rpm", 20);
        const int secs = xmcli::PromptInt("seconds", 2);
        if (xmcli::PromptYesNo("WHEELS OFF THE GROUND — spin now?", false))
          DdsmSpin(motor, static_cast<double>(rpm), static_cast<double>(secs));
        break;
      }
      case 3:
        motor.Stop();
        std::cout << "  stopped\n";
        break;
    }
  }
  return 0;
}
}  // namespace

int xmcli::RunDdsm(const Args& args) {
  const std::string port = ResolvePort(args);
  if (port.empty()) return 2;

  int id = 1;
  if (args.has("id")) {
    if (!ParseInt(args.get("id"), id)) {
      std::cerr << "ddsm: bad --id\n";
      return 2;
    }
  } else {
    id = PromptInt("motor id", 1);
  }

  Ddsm210::Config cfg;
  cfg.bus = port;
  cfg.id = static_cast<std::uint8_t>(id);
  Ddsm210 motor(cfg);

  std::cout << "ddsm: connecting id " << id << " on " << port << " ...\n";
  if (motor.Connect() != hal::Status::kOk) {
    std::cerr << "ddsm: FAILED to open " << port << "\n";
    return 1;
  }

  // No action flag on a terminal => interactive console (switch mode / spin /
  // stop from a menu). Flags (--set-mode / --spin) keep the one-shot path.
  if (!args.has("spin") && !args.has("set-mode") && StdinIsTty()) {
    const int rc = DdsmInteractive(motor, id);
    motor.Disconnect();
    return rc;
  }

  // Read-only feedback.
  motor.RequestModeFeedback();
  motor.RequestOdometryFeedback();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  Ddsm210::Mode mode = motor.GetMode();
  const auto speed = motor.GetSpeed();
  std::cout << "  mode=" << DdsmModeName(mode) << "  speed="
            << (speed.ok() ? std::to_string(speed.value.value())
                           : std::string("stale"))
            << " rpm  encoder=" << motor.GetEncoderCount() << "\n";

  // Optional --set-mode speed|position|open-loop: sends the runtime mode-switch
  // command (0xA0) and polls for confirmation. It is not motion and not a
  // persistent burn — the DDSM-210 comes up in its power-on default mode, so a
  // switch does not survive a power cycle — hence no --confirm is required.
  if (args.has("set-mode")) {
    Ddsm210::Mode target = Ddsm210::Mode::kUnknown;
    if (!ParseDdsmMode(args.get("set-mode"), target)) {
      std::cerr << "ddsm: bad --set-mode (use speed|position|open-loop)\n";
      motor.Disconnect();
      return 2;
    }
    std::cout << "ddsm: switching id " << id << " to " << DdsmModeName(target)
              << " mode ...\n";
    if (motor.SetMode(target) != hal::Status::kOk) {
      std::cerr << "ddsm: mode switch not confirmed (wrong id? wiring?)\n";
      motor.Disconnect();
      return 1;
    }
    motor.RequestModeFeedback();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    mode = motor.GetMode();
    std::cout << "ddsm: mode now " << DdsmModeName(mode) << "\n";
  }

  // Optional motion.
  if (args.has("spin")) {
    double rpm = 0.0;
    if (!ParseDouble(args.get("spin"), rpm)) {
      std::cerr << "ddsm: bad --spin\n";
      motor.Disconnect();
      return 2;
    }
    const bool go =
        args.has("confirm") ||
        PromptYesNo("Spin motor at " + std::to_string(rpm) +
                        " rpm? WHEELS OFF THE GROUND",
                    false);
    if (!go) {
      std::cout << "ddsm: aborted\n";
      motor.Disconnect();
      return 0;
    }
    double secs = 2.0;
    if (args.has("seconds")) ParseDouble(args.get("seconds"), secs);
    DdsmSpin(motor, rpm, secs);  // switches to speed mode if needed
  }

  motor.Disconnect();
  return 0;
}

/*
 * cmd_servo.cpp — Waveshare SMS/STS bus servo: read state, switch mode, move.
 *
 * With no action flag on a terminal, drops into an interactive console
 * (refresh / switch mode / move / set-neutral / stop). Flags stay one-shot and
 * scriptable: --set-mode speed|position, --move DEG (gated by --confirm).
 *
 * Note: the SmsStsServo API has no mode getter, so the console cannot show or
 * auto-switch the current mode — SetPosition needs position mode, and is simply
 * reported as rejected if the servo is in the wrong one. Set it explicitly.
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
#include "motor_waveshare/sms_sts_servo.hpp"

using namespace xmotion;

namespace {
constexpr double kPi = 3.14159265358979323846;

const char* ServoModeName(SmsStsServo::Mode m) {
  switch (m) {
    case SmsStsServo::Mode::kSpeed:
      return "speed";
    case SmsStsServo::Mode::kPosition:
      return "position";
    case SmsStsServo::Mode::kUnknown:
      return "unknown";
  }
  return "?";
}

bool ParseServoMode(const std::string& s, SmsStsServo::Mode& out) {
  if (s == "speed") {
    out = SmsStsServo::Mode::kSpeed;
  } else if (s == "position") {
    out = SmsStsServo::Mode::kPosition;
  } else {
    return false;
  }
  return true;
}

void PrintServoState(SmsStsServo& servo) {
  const auto st = servo.GetState();
  if (st.ok()) {
    const auto& s = st.value;
    std::cout << "  pos=" << s.position << " deg  moving="
              << (s.is_moving ? "yes" : "no") << "  volt=" << s.voltage
              << "  temp=" << s.temperature << " C  load=" << s.load << "\n";
  } else {
    std::cout << "  state: " << hal::ToString(st.status)
              << " (no fresh feedback — id / wiring / baud?)\n";
  }
}

void ServoMove(SmsStsServo& servo, double deg) {
  std::cout << "  moving to " << deg << " deg ...\n";
  if (servo.SetPosition(hal::Radian{deg * kPi / 180.0}) != hal::Status::kOk)
    std::cout << "  SetPosition rejected (in position mode? within range?)\n";
  std::this_thread::sleep_for(std::chrono::milliseconds(600));
  PrintServoState(servo);
}

// Interactive console: refresh feedback and pick switch-mode / move / neutral /
// stop from a menu. Runs until the user quits or Ctrl-C.
int ServoInteractive(SmsStsServo& servo, int id) {
  while (!xmcli::g_stop.load()) {
    std::cout << "\n[servo id " << id << "]\n";
    PrintServoState(servo);

    const int choice = xmcli::PromptChoice(
        "action:", {"refresh feedback", "switch mode (speed/position)",
                    "move to angle", "set neutral (current pos = center)",
                    "stop", "quit"});
    if (choice < 0 || choice == 5) break;  // EOF or quit

    switch (choice) {
      case 0:
        break;  // loop refreshes feedback
      case 1: {
        const int m = xmcli::PromptChoice("switch to:", {"speed", "position"});
        if (m < 0) break;
        const SmsStsServo::Mode target = (m == 0)
                                             ? SmsStsServo::Mode::kSpeed
                                             : SmsStsServo::Mode::kPosition;
        std::cout << "  switching to " << ServoModeName(target) << " ...\n";
        if (servo.SetMode(target) != hal::Status::kOk)
          std::cout << "  mode switch failed (id / wiring?)\n";
        else
          std::cout << "  ok\n";
        break;
      }
      case 2: {
        const int deg = xmcli::PromptInt("angle (deg)", 0);
        if (xmcli::PromptYesNo("Move servo to " + std::to_string(deg) + " deg?",
                               false))
          ServoMove(servo, static_cast<double>(deg));
        break;
      }
      case 3: {
        if (xmcli::PromptYesNo("Set current position as neutral (center)?",
                               false)) {
          if (servo.SetNeutralPosition() != hal::Status::kOk)
            std::cout << "  set-neutral failed\n";
          else
            std::cout << "  neutral set\n";
        }
        break;
      }
      case 4:
        servo.Stop();
        std::cout << "  stopped\n";
        break;
    }
  }
  return 0;
}
}  // namespace

int xmcli::RunServo(const Args& args) {
  const std::string port = ResolvePort(args);
  if (port.empty()) return 2;

  int id = 1;
  if (args.has("id")) {
    if (!ParseInt(args.get("id"), id)) {
      std::cerr << "servo: bad --id\n";
      return 2;
    }
  } else {
    id = PromptInt("servo id", 1);
  }

  SmsStsServo::Config cfg;
  cfg.bus = port;
  cfg.id = static_cast<std::uint8_t>(id);
  if (args.has("baud")) {
    int b = 0;
    if (ParseInt(args.get("baud"), b)) cfg.baud = b;
  }
  SmsStsServo servo(cfg);

  std::cout << "servo: connecting id " << id << " on " << port << " @ "
            << cfg.baud << " baud ...\n";
  if (servo.Connect() != hal::Status::kOk) {
    std::cerr << "servo: FAILED to open " << port << "\n";
    return 1;
  }

  // No action flag on a terminal => interactive console (switch mode / move /
  // set-neutral / stop). Flags (--set-mode / --move) keep the one-shot path.
  if (!args.has("move") && !args.has("set-mode") && StdinIsTty()) {
    const int rc = ServoInteractive(servo, id);
    servo.Disconnect();
    return rc;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  PrintServoState(servo);

  // Optional --set-mode speed|position: runtime mode switch (see ddsm note).
  if (args.has("set-mode")) {
    SmsStsServo::Mode target = SmsStsServo::Mode::kUnknown;
    if (!ParseServoMode(args.get("set-mode"), target)) {
      std::cerr << "servo: bad --set-mode (use speed|position)\n";
      servo.Disconnect();
      return 2;
    }
    std::cout << "servo: switching id " << id << " to " << ServoModeName(target)
              << " mode ...\n";
    if (servo.SetMode(target) != hal::Status::kOk) {
      std::cerr << "servo: mode switch failed (id / wiring?)\n";
      servo.Disconnect();
      return 1;
    }
    std::cout << "servo: mode set\n";
  }

  // Optional motion.
  if (args.has("move")) {
    double deg = 0.0;
    if (!ParseDouble(args.get("move"), deg)) {
      std::cerr << "servo: bad --move\n";
      servo.Disconnect();
      return 2;
    }
    const bool go = args.has("confirm") ||
                    PromptYesNo("Move servo to " + std::to_string(deg) + " deg?",
                                false);
    if (!go) {
      std::cout << "servo: aborted\n";
      servo.Disconnect();
      return 0;
    }
    ServoMove(servo, deg);
  }

  servo.Disconnect();
  return 0;
}

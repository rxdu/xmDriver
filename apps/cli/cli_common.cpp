/*
 * cli_common.cpp
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "cli_common.hpp"

#include <glob.h>
#include <signal.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>

namespace xmcli {

std::atomic<bool> g_stop{false};

namespace {
void OnSignal(int /*sig*/) { g_stop.store(true); }

void GlobInto(const char* pattern, std::vector<std::string>& out) {
  glob_t g{};
  if (glob(pattern, 0, nullptr, &g) == 0) {
    for (std::size_t i = 0; i < g.gl_pathc; ++i) out.emplace_back(g.gl_pathv[i]);
  }
  globfree(&g);
}
}  // namespace

void InstallSignalHandler() {
  // Deliberately NO SA_RESTART: without it a blocking read (e.g. std::getline
  // at a menu prompt) is interrupted by Ctrl-C and returns, so the interactive
  // loops can unwind and exit — not just the streaming ones that poll g_stop.
  struct sigaction sa;
  sa.sa_handler = OnSignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
}

bool StdinIsTty() { return ::isatty(STDIN_FILENO) == 1; }

Args ParseArgs(int argc, char** argv) {
  Args a;
  int i = 1;
  if (i < argc && argv[i][0] != '-') a.device = argv[i++];
  for (; i < argc; ++i) {
    const std::string tok = argv[i];
    if (tok.rfind("--", 0) != 0) continue;
    const std::string key = tok.substr(2);
    // A following token that is not itself a `--flag` is this key's value;
    // otherwise treat it as a boolean flag (e.g. --confirm). A single-dash
    // token (e.g. a negative number "-20") counts as a value.
    if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
      a.opts[key] = argv[++i];
    } else {
      a.opts[key] = "true";
    }
  }
  return a;
}

bool ParseInt(const std::string& s, int& out) {
  if (s.empty()) return false;
  char* end = nullptr;
  const long v = std::strtol(s.c_str(), &end, 10);
  if (*end != '\0') return false;
  out = static_cast<int>(v);
  return true;
}

bool ParseDouble(const std::string& s, double& out) {
  if (s.empty()) return false;
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (*end != '\0') return false;
  out = v;
  return true;
}

int PromptChoice(const std::string& title,
                 const std::vector<std::string>& options) {
  if (!StdinIsTty() || options.empty()) return -1;
  for (;;) {
    std::cout << "\n" << title << "\n";
    for (std::size_t i = 0; i < options.size(); ++i)
      std::cout << "  " << (i + 1) << ") " << options[i] << "\n";
    std::cout << "select [1-" << options.size() << "]: " << std::flush;
    std::string line;
    if (g_stop.load() || !std::getline(std::cin, line)) return -1;
    int n = 0;
    if (ParseInt(line, n) && n >= 1 && n <= static_cast<int>(options.size()))
      return n - 1;
    std::cout << "  invalid choice\n";
  }
}

std::string PromptLine(const std::string& prompt, const std::string& def) {
  if (!StdinIsTty()) return def;
  std::cout << prompt << " [" << def << "]: " << std::flush;
  std::string line;
  if (g_stop.load() || !std::getline(std::cin, line) || line.empty()) return def;
  return line;
}

int PromptInt(const std::string& prompt, int def) {
  for (;;) {
    const std::string s = PromptLine(prompt, std::to_string(def));
    int v = 0;
    if (ParseInt(s, v)) return v;
    if (!StdinIsTty()) return def;
    std::cout << "  not a number\n";
  }
}

bool PromptYesNo(const std::string& prompt, bool def) {
  if (!StdinIsTty()) return def;
  std::cout << prompt << (def ? " [Y/n]: " : " [y/N]: ") << std::flush;
  std::string line;
  if (g_stop.load() || !std::getline(std::cin, line) || line.empty()) return def;
  return line[0] == 'y' || line[0] == 'Y';
}

std::vector<std::string> ListSerialPorts() {
  std::vector<std::string> ports;
  GlobInto("/dev/ttyACM*", ports);
  GlobInto("/dev/ttyUSB*", ports);
  GlobInto("/dev/serial/by-id/*", ports);
  return ports;
}

std::string ResolvePort(const Args& args) {
  if (args.has("port")) return args.get("port");
  const std::vector<std::string> ports = ListSerialPorts();
  if (ports.empty()) {
    std::cerr << "no serial ports found (/dev/ttyACM*, /dev/ttyUSB*). "
                 "Pass --port explicitly.\n";
    return "";
  }
  if (!StdinIsTty()) {
    std::cerr << "no --port given and stdin is not a terminal.\n";
    return "";
  }
  const int idx = PromptChoice("Select a serial port:", ports);
  return idx < 0 ? std::string() : ports[idx];
}

}  // namespace xmcli

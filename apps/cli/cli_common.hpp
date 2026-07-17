/*
 * cli_common.hpp
 *
 * Shared helpers for xmdriver_cli: argument parsing, interactive prompts
 * (device / serial-port selection), serial-port discovery, and a Ctrl-C stop
 * flag. Interactive prompts only fire on a TTY; with stdin redirected the CLI
 * stays non-interactive (flags only) so it is scriptable.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#ifndef XMDRIVER_CLI_COMMON_HPP
#define XMDRIVER_CLI_COMMON_HPP

#include <atomic>
#include <map>
#include <string>
#include <vector>

namespace xmcli {

// Positional device (argv[1]) + `--key value` / `--flag` options.
struct Args {
  std::string device;
  std::map<std::string, std::string> opts;

  bool has(const std::string& k) const { return opts.find(k) != opts.end(); }
  std::string get(const std::string& k, const std::string& def = "") const {
    auto it = opts.find(k);
    return it == opts.end() ? def : it->second;
  }
};

Args ParseArgs(int argc, char** argv);

bool StdinIsTty();

// Numbered menu; returns the chosen 0-based index, or -1 if not a TTY / EOF.
int PromptChoice(const std::string& title,
                 const std::vector<std::string>& options);
// Free-form line with a default (empty input or non-TTY returns `def`).
std::string PromptLine(const std::string& prompt, const std::string& def);
int PromptInt(const std::string& prompt, int def);
bool PromptYesNo(const std::string& prompt, bool def);

// Detected serial devices: /dev/ttyACM*, /dev/ttyUSB*, /dev/serial/by-id/*.
std::vector<std::string> ListSerialPorts();
// --port if given; else an interactive pick from the detected ports. Returns
// "" when unresolved (no --port and not a TTY, no ports, or aborted).
std::string ResolvePort(const Args& args);

// Strict numeric parse (false on any trailing junk / empty).
bool ParseInt(const std::string& s, int& out);
bool ParseDouble(const std::string& s, double& out);

// Set by SIGINT/SIGTERM; streaming commands poll it to exit cleanly.
extern std::atomic<bool> g_stop;
void InstallSignalHandler();

}  // namespace xmcli

#endif  // XMDRIVER_CLI_COMMON_HPP

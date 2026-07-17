/*
 * commands.hpp — per-device subcommand entry points for xmdriver_cli.
 *
 * Each returns a process exit code (0 = ok). Adding a device = add a Run*()
 * here, its cmd_*.cpp, and a row in the dispatch table in xmdriver_cli.cpp.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#ifndef XMDRIVER_CLI_COMMANDS_HPP
#define XMDRIVER_CLI_COMMANDS_HPP

#include "cli_common.hpp"

namespace xmcli {

int RunSbus(const Args& args);   // SBUS RC receiver — live channels
int RunDdsm(const Args& args);   // DDSM-210 hub motor — feedback / spin
int RunServo(const Args& args);  // SMS/STS bus servo — feedback / move
int RunImu(const Args& args);    // HiPNUC IMU — live accel/gyro/orientation

}  // namespace xmcli

#endif  // XMDRIVER_CLI_COMMANDS_HPP

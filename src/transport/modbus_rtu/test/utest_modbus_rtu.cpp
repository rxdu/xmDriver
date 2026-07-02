/*
 * utest_modbus_rtu.cpp — hardware-free lifecycle/RAII test for ModbusRtuPort.
 *
 * ModbusRtuPort owns a raw libmodbus context (modbus_t*). ADR 0002 hardened the
 * wrapper against a context leak, a double modbus_free, and an un-nulled context
 * after Close(). These properties are all observable WITHOUT a real serial bus:
 * construction allocates nothing until Open(), Open() against a bogus device
 * fails cleanly and leaves the port closed, Close() is idempotent, Open/Close
 * can be repeated without double-freeing, and read/write on a not-connected port
 * returns an error instead of dereferencing a null context. Run under ASan/LSan
 * to also catch the leak/double-free directly.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#include "gtest/gtest.h"

#include "modbus_rtu/modbus_rtu_port.hpp"

using namespace xmotion;

namespace {

// A device path guaranteed not to exist, so modbus_connect() always fails and
// no real hardware is touched.
constexpr const char* kBogusDevice = "/dev/xm_nonexistent_ttyUSB";
constexpr int kBaudRate = 115200;

// Open() with the standard 8N1 framing against the bogus device.
bool OpenBogus(ModbusRtuPort& port) {
  return port.Open(kBogusDevice, kBaudRate,
                   ModbusRtuInterface::Parity::kNone,
                   ModbusRtuInterface::DataBit::kBit8,
                   ModbusRtuInterface::StopBit::kBit1);
}

// Construct then destruct with no Open/Connect: the dtor must free the context
// allocated (or not) without crashing. ASan/LSan turns any leak into a failure.
TEST(ModbusRtuPortLifecycle, ConstructDestructNoOpen) {
  { ModbusRtuPort port; }
  SUCCEED();
}

// A freshly constructed port owns no connection.
TEST(ModbusRtuPortLifecycle, NotOpenedAfterConstruction) {
  ModbusRtuPort port;
  EXPECT_FALSE(port.IsOpened());
}

// Open() against a bogus device returns false, does not crash, and leaves the
// port not-open (the failure path frees the context via Close()).
TEST(ModbusRtuPortLifecycle, OpenBogusDeviceFailsClean) {
  ModbusRtuPort port;
  EXPECT_FALSE(OpenBogus(port));
  EXPECT_FALSE(port.IsOpened());
}

// Close() on a never-opened port is a no-op (guards the null context).
TEST(ModbusRtuPortLifecycle, CloseWithoutOpenIsSafe) {
  ModbusRtuPort port;
  port.Close();
  EXPECT_FALSE(port.IsOpened());
}

// Close() is idempotent: a second Close() must not double-free. The un-nulled
// context bug would have modbus_free()'d a dangling pointer here.
TEST(ModbusRtuPortLifecycle, DoubleCloseIsSafe) {
  ModbusRtuPort port;
  EXPECT_FALSE(OpenBogus(port));  // allocates then frees the context
  port.Close();
  port.Close();
  EXPECT_FALSE(port.IsOpened());
}

// Reopen after Close: repeated Open(bogus)/Close() cycles must not leak or
// double-free the context. Open() itself begins by Close()ing any stale state.
TEST(ModbusRtuPortLifecycle, ReopenAfterCloseNoLeak) {
  ModbusRtuPort port;
  for (int i = 0; i < 3; ++i) {
    EXPECT_FALSE(OpenBogus(port));
    EXPECT_FALSE(port.IsOpened());
    port.Close();
    EXPECT_FALSE(port.IsOpened());
  }
}

// SetResponseTimeout() on a not-initialized context returns false rather than
// dereferencing null.
TEST(ModbusRtuPortLifecycle, SetResponseTimeoutWhileClosedFails) {
  ModbusRtuPort port;
  EXPECT_FALSE(port.SetResponseTimeout(0, 500000));
}

// Every read/write on a not-connected port returns the -1 error status instead
// of segfaulting on a null context.
TEST(ModbusRtuPortReadWrite, ReadWriteWhileNotConnectedReturnsError) {
  ModbusRtuPort port;
  ASSERT_FALSE(port.IsOpened());

  uint8_t bits[4] = {0};
  uint16_t regs[4] = {0};
  const uint8_t out_bits[4] = {1, 0, 1, 0};
  const uint16_t out_regs[4] = {1, 2, 3, 4};

  EXPECT_EQ(port.ReadCoils(1, 0, 4, bits), -1);
  EXPECT_EQ(port.WriteSingleCoil(1, 0, 1), -1);
  EXPECT_EQ(port.WriteMultipleCoils(1, 0, 4, out_bits), -1);
  EXPECT_EQ(port.ReadDiscreteInputs(1, 0, 4, bits), -1);
  EXPECT_EQ(port.ReadInputRegisters(1, 0, 4, regs), -1);
  EXPECT_EQ(port.ReadHoldingRegisters(1, 0, 4, regs), -1);
  EXPECT_EQ(port.WriteSingleRegister(1, 0, 42), -1);
  EXPECT_EQ(port.WriteMultipleRegisters(1, 0, 4, out_regs), -1);
}

// Read/write still return the error status (never a stale success) after a
// failed Open() followed by Close().
TEST(ModbusRtuPortReadWrite, ReadWriteAfterFailedOpenReturnsError) {
  ModbusRtuPort port;
  EXPECT_FALSE(OpenBogus(port));
  port.Close();

  uint16_t regs[2] = {0};
  EXPECT_EQ(port.ReadHoldingRegisters(1, 0, 2, regs), -1);
  EXPECT_EQ(port.WriteSingleRegister(1, 0, 7), -1);
}

}  // namespace

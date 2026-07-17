/*
 * serial_baud_linux.cpp
 *
 * Description: arbitrary (non-standard) serial baud via the Linux termios2 /
 *   BOTHER ioctl. Kept in its own translation unit because <asm/termbits.h>
 *   (struct termios2) cannot coexist with the glibc <termios.h> that asio pulls
 *   into async_serial.cpp — including both redefines struct termios.
 *
 * Copyright (c) 2026 Ruixiang Du (rdu)
 */

#if defined(__linux__)
#include <asm/termbits.h>  // struct termios2, BOTHER, CBAUD
#include <sys/ioctl.h>     // ioctl(), TCGETS2, TCSETS2

namespace xmotion {
// Program an arbitrary baud (e.g. SBUS's non-standard 100000) with termios2.
// Unlike the legacy ASYNC_SPD_CUST custom-divisor path, this asks the device
// for the literal rate, so it works where no UART baud_base is exposed —
// notably cdc_acm USB serial (/dev/ttyACM*), whose baud_base reads 0. Only the
// baud fields are touched; the caller's byte format (parity/stop/data bits) is
// read back and preserved. Returns true on success.
bool SetSerialBaudTermios2(int fd, unsigned baud) {
  struct termios2 tio;
  if (ioctl(fd, TCGETS2, &tio) != 0) return false;
  tio.c_cflag &= ~CBAUD;
  tio.c_cflag |= BOTHER;
  tio.c_ispeed = baud;
  tio.c_ospeed = baud;
  return ioctl(fd, TCSETS2, &tio) == 0;
}
}  // namespace xmotion
#endif  // __linux__

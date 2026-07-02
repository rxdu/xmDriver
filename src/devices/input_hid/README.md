# HID Device Driver

This module provides HAL drivers for the following HID input devices:

- `xmotion::EvdevKeyboard` — implements `xmotion::hal::Keyboard`
- `xmotion::EvdevJoystick` — implements `xmotion::hal::Joystick`

Both are single, event-driven implementations built on **libevdev** (ADR 0002)
and the canonical `xmotion::hal` device contract (ADR 0001 + 0003):

- Lifecycle via `Connect()` / `Disconnect()` / `IsConnected()` / `Health()`.
- `Read()` returns a `Result<T>` snapshot: `kNotConnected` when down, `kTimeout`
  when the device has stopped reporting within the freshness window, and a real
  value (buttons/keys + axes normalised to `[-1, 1]` from the device's own abs
  ranges, plus a `steady_clock` stamp) only when fresh.
- Event callbacks: `SetButtonCallback` / `SetAxisCallback` (joystick),
  `SetKeyEventCallback` (keyboard).
- A hot-unplug is observable: the reader detects the device is gone and
  `Health()` reports it. There is no inotify hot-plug re-attach.

Each device runs one background reader thread that waits on the fd with `epoll`
and drains events with libevdev (including `SYN_DROPPED` resync). The device fd
is only ever touched on that reader thread, so `Read()`/`Health()` are race-free.
Construct with the event node path, e.g. `EvdevKeyboard{"/dev/input/event3"}`.

## Find the device

An easy way to find the device is to use the `evtest` tool.

```bash
$ sudo apt-get install evtest
$ sudo evtest
```

You will get a list of devices. Find the one that corresponds to the device you want to use.

```bash
No device specified, trying to scan all of /dev/input/event*
Available devices:
/dev/input/event0:	Sleep Button
/dev/input/event1:	Power Button
/dev/input/event2:	Power Button
/dev/input/event3:	HID Keyboard HID Keyboard
/dev/input/event4:	HID Keyboard HID Keyboard
/dev/input/event6:	MSI WMI hotkeys
/dev/input/event7:	G2Touch Multi-Touch
/dev/input/event8:	USB 2.0 Camera: HD USB Camera
/dev/input/event9:	HDA NVidia HDMI/DP,pcm=3
/dev/input/event10:	Logitech USB Receiver
/dev/input/event11:	Logitech USB Receiver Mouse
/dev/input/event12:	Logitech USB Receiver Consumer Control
/dev/input/event13:	Logitech USB Receiver System Control
/dev/input/event14:	HDA NVidia HDMI/DP,pcm=7
/dev/input/event15:	HDA NVidia HDMI/DP,pcm=8
/dev/input/event16:	HDA NVidia HDMI/DP,pcm=9
/dev/input/event17:	HDA Intel PCH Front Mic
/dev/input/event18:	HDA Intel PCH Rear Mic
/dev/input/event19:	HDA Intel PCH Line
/dev/input/event20:	HDA Intel PCH Headphone Front
/dev/input/event21:	HDA Intel PCH Front Headphone Surround
```

## Device permission

You can check the permission of the device with the `ls` command.

```bash
$ ls -la /dev/input/event8 
crw-rw---- 1 root input 13, 72 Oct 19  2024 /dev/input/event8
```

The device is owned by the `root` user and the `input` group. You can add your user to the `input` group to get access
to the device.

```bash
$ sudo usermod -a -G input $USER
```
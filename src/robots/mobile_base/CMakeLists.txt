# mobile_base — the reusable Mobile-Base CAN protocol (spec: docs/can_protocol.md).
#
# A self-authored device protocol: structured like the vendor device protocols
# under src/devices/ (DDSM, VESC, SBUS), but owned by us and frozen at wire
# version 1.0.0. Header-only today — the Core profile codec, E2E integrity, the
# base-side command tracker, and opt-in model profiles (profiles/swerve_v1).
# Depends only on the first-party CanFrame at the transport boundary, so it is
# transport-agnostic and MCU-portable.
xmdriver_add_module(mobile_base INTERFACE
    PUBLIC_DEPS xmotion::transport_interface)

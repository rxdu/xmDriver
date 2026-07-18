# mobile_base — the abstract Mobile-Base CONTRACT (no transport). Two header-only
# targets, layered so a realization takes only what it needs:
#
#   mobile_base_types   the SI vocabulary (types.hpp): command/state structs,
#                       enums, capability/fault bits. Dependency-free (stdlib
#                       only) — an MCU codec or a host base links just this.
#   mobile_base         the abstract MobileBase interface (mobile_base.hpp), i.e.
#                       the vocabulary + hal::Device. Navigation and each
#                       implementation depend on this.
#
# The CAN realization lives in the sibling mobile_base_can/; a host-composed base
# (e.g. ackermann_vesc/) is another sibling. Both implement this contract.
xmdriver_add_module(mobile_base_types INTERFACE)

xmdriver_add_module(mobile_base INTERFACE
    PUBLIC_DEPS mobile_base_types xmotion::hal)

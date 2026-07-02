# Transport layer: the communication-channel abstraction (interfaces) and its
# implementations. Grouped here so the layer is visible in the tree; they remain
# SEPARATE targets so their disjoint heavy deps don't couple — a consumer links
# only what it needs (async_port -> asio, modbus_rtu -> libmodbus), and the
# interface target stays dependency-free. See docs/adr/0001.
#
# interface first (impls depend on it).
add_subdirectory(interface)
add_subdirectory(async_port)
add_subdirectory(modbus_rtu)

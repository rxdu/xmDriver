# Transport interface layer — the abstract communication-channel contracts
# (serial / CAN / Modbus-RTU) that transport implementations (async_port,
# modbus_rtu) realize and device drivers consume. Header-only, std-only; it is a
# peer layer BELOW the device HAL and must not depend on hal (no back-edge).
xmmu_add_module(transport_interface INTERFACE)

# hal/imu.hpp uses the geometry tier (Quaterniond) — since xmBase 0.5.0 the
# Eigen-backed types live behind the separate xmBaseGeometry target.
xmdriver_add_module(hal INTERFACE PUBLIC_DEPS xmotion::xmBase xmotion::xmBaseGeometry)

xmmu_add_module(motor_registry
    SOURCES src/register_all.cpp
    # Links every motor driver so RegisterAllMotors() forces their TUs (and thus
    # their factory registration) into the link — robust against static-lib GC.
    PUBLIC_DEPS motor_vesc motor_akelc motor_waveshare)

# Robot-layer drivers — a whole robot/base addressed as one entity over a bus,
# as opposed to the physical-component drivers under src/devices/. Each is an
# independently-linkable library (xmotion::<module>).
add_subdirectory(mobile_base)      # the contract (interface + vocabulary)
add_subdirectory(mobile_base_can)  # the CAN realization (depends on the contract)

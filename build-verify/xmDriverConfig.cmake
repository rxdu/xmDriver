include(CMakeFindDependencyMacro)

# Capturing values from configure (optional)
# set(my-config-var )

# Same syntax as find_package
find_dependency(Threads REQUIRED)

# xmDriver is built on top of xmBase, which provides the xmotion::xmBase
# target that every driver module links against.
find_dependency(xmBase REQUIRED)

# Any extra setup

# Add the targets file
include("${CMAKE_CURRENT_LIST_DIR}/xmDriverTargets.cmake")

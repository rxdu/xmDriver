cmake_minimum_required(VERSION 3.10.2)
project(xmDriver VERSION 0.1.0)

message(STATUS "------------------------------------------------")
if (PROJECT_NAME STREQUAL CMAKE_PROJECT_NAME)
  set(BUILD_AS_MODULE OFF)
  message(STATUS "Build xmDriver as a standalone project")
else ()
  set(BUILD_AS_MODULE ON)
  message(STATUS "Build xmDriver as a module")
endif ()
message(STATUS "------------------------------------------------")

find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
  message(STATUS "Found ccache")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
endif ()

## Project Options
option(BUILD_TESTING "Build tests" OFF)
option(STATIC_CHECK "Enable static check" OFF)
option(XMOTION_DEV_MODE "Development mode forces building tests" OFF)

## Generate symbols for IDE indexer
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (STATIC_CHECK)
  find_program(CPPCHECK cppcheck)
  if (CPPCHECK)
    message(STATUS "Found cppcheck")
    set(CMAKE_CXX_CPPCHECK cppcheck;--std=c++17;--enable=all)
  endif ()
endif ()

## Additional cmake module path
set(USER_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${USER_CMAKE_PATH}/modules")
list(APPEND CMAKE_PREFIX_PATH "/usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu/cmake")

## Set compiler to use c++ 17 features
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

## Use GNUInstallDirs to install libraries into correct locations on all platforms.
include(GNUInstallDirs)

## Put all binary files into /bin and libraries into /lib
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
if (XMOTION_DEV_MODE)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
else ()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
endif ()

if (XMOTION_DEV_MODE)
  message(STATUS "xmDriver development mode enabled")
endif ()

## Choose build type
set(default_build_type "Release")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)

  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
      "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif ()

## Enable test if requested
if (((NOT BUILD_AS_MODULE) AND BUILD_TESTING) OR XMOTION_DEV_MODE)
  enable_testing()
  include(GoogleTest)
  set(BUILD_TESTS ON)
  message(STATUS "xmDriver tests will be built")
else ()
  set(BUILD_TESTS OFF)
  message(STATUS "xmDriver tests will not be built")
endif ()

## Enable -fPIC for all targets
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

## Foundation dependency: xmBase. If a parent superbuild already provides it
## in-tree (xmotion::xmBase exists), use that and skip — this avoids adding
## xmBase twice when xmDriver is assembled alongside it (e.g. in xmNabla or the
## umbrella). Otherwise prefer an installed xmBase, falling back to the bundled
## third_party/xmBase submodule so xmDriver still builds standalone out of the box.
## Either path provides xmotion::interface / xmotion::logging / xmotion::xmBase.
if (NOT TARGET xmotion::xmBase)
  find_package(xmBase QUIET)
  if (NOT xmBase_FOUND)
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/xmBase/CMakeLists.txt)
      message(STATUS "xmDriver: using bundled third_party/xmBase")
      add_subdirectory(third_party/xmBase)
    else ()
      message(FATAL_ERROR "xmDriver: xmBase not found — install xmBase or init the third_party/xmBase submodule")
    endif ()
  endif ()
endif ()

## Dependent libraries (googletest for tests, rpi_sbus for input_sbus)
add_subdirectory(third_party)

## Per-module CMake helpers (xmdriver_add_module / xmdriver_add_group)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/xmdriver_module.cmake)

## Add source directories
add_subdirectory(src)

## Installation setup
# Offer the user the choice of overriding the installation directories
set(INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR} CACHE PATH "Installation directory for libraries")
set(INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR} CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR} CACHE PATH "Installation directory for header files")
if (WIN32 AND NOT CYGWIN)
  set(DEF_INSTALL_CMAKEDIR CMake)
else ()
  set(DEF_INSTALL_CMAKEDIR share/cmake/${PROJECT_NAME})
endif ()
set(INSTALL_CMAKEDIR ${DEF_INSTALL_CMAKEDIR} CACHE PATH "Installation directory for CMake files")

# print installation paths
message(STATUS "Project will be installed to ${CMAKE_INSTALL_PREFIX} with 'make install'")
foreach (p LIB BIN INCLUDE CMAKE)
  file(TO_NATIVE_PATH ${CMAKE_INSTALL_PREFIX}/${INSTALL_${p}DIR} _path)
  message(STATUS " - To install ${p} components to ${_path}")
  unset(_path)
endforeach ()

# export target configuration
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    xmDriverConfigVersion.cmake
    VERSION "${${PROJECT_NAME}_VERSION_MAJOR}.${${PROJECT_NAME}_VERSION_MINOR}.${${PROJECT_NAME}_VERSION_PATCH}"
    COMPATIBILITY SameMajorVersion)

install(EXPORT xmDriverTargets
    FILE xmDriverTargets.cmake
    NAMESPACE xmotion::
    DESTINATION lib/cmake/xmDriver)

configure_file(cmake/xmDriverConfig.cmake.in xmDriverConfig.cmake @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/xmDriverConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/xmDriverConfigVersion.cmake"
    DESTINATION lib/cmake/xmDriver)

# Packaging support
set(CPACK_DEBIAN_PACKAGE_NAME "libxmotion-driver")
set(CPACK_PACKAGE_VENDOR "Ruixiang Du")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "XMotion Mu - host hardware drivers for the XMotion family")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/xmotion")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libxmotion-sigma, libasio-dev, libmodbus-dev, libevdev-dev")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ruixiang Du (ruixiang.du@gmail.com)")
set(CPACK_SOURCE_IGNORE_FILES .git dist .*build.* /\\\\.DS_Store)
include(CPack)

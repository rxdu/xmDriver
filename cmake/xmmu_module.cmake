# xmmu_module.cmake — helpers that remove the repeated per-module boilerplate
# (add_library / include dirs / install / export / alias / test wiring).
#
# Global -fPIC comes from CMAKE_POSITION_INDEPENDENT_CODE (set at the top level),
# so modules no longer set it individually.

# xmmu_add_module(<name>
#   [INTERFACE]                      # header-only module (no sources)
#   [SOURCES <s>...]                 # sources for a STATIC library
#   [PUBLIC_DEPS <t>...]             # PUBLIC link dependencies
#   [PUBLIC_DEFINITIONS <d>...]      # PUBLIC compile definitions
#   [PRIVATE_DEFINITIONS <d>...]     # PRIVATE compile definitions
#   [PRIVATE_INCLUDE_DIRS <dir>...]  # extra PRIVATE include dirs (src/ added by default)
# )
#
# Creates target <name> and the xmotion::<name> alias, wires the standard
# include layout (BUILD vs INSTALL interface), installs + exports to xmMuTargets,
# installs include/, and adds the module's test/ dir when BUILD_TESTS is on.
function(xmmu_add_module name)
  cmake_parse_arguments(ARG
      "INTERFACE"
      ""
      "SOURCES;PUBLIC_DEPS;PUBLIC_DEFINITIONS;PRIVATE_DEFINITIONS;PRIVATE_INCLUDE_DIRS"
      ${ARGN})

  if (ARG_INTERFACE)
    add_library(${name} INTERFACE)
    target_include_directories(${name} INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    if (ARG_PUBLIC_DEPS)
      target_link_libraries(${name} INTERFACE ${ARG_PUBLIC_DEPS})
    endif ()
  else ()
    add_library(${name} STATIC ${ARG_SOURCES})
    target_include_directories(${name} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${ARG_PRIVATE_INCLUDE_DIRS})
    if (ARG_PUBLIC_DEPS)
      target_link_libraries(${name} PUBLIC ${ARG_PUBLIC_DEPS})
    endif ()
    if (ARG_PUBLIC_DEFINITIONS)
      target_compile_definitions(${name} PUBLIC ${ARG_PUBLIC_DEFINITIONS})
    endif ()
    if (ARG_PRIVATE_DEFINITIONS)
      target_compile_definitions(${name} PRIVATE ${ARG_PRIVATE_DEFINITIONS})
    endif ()
  endif ()

  add_library(xmotion::${name} ALIAS ${name})

  install(TARGETS ${name}
      EXPORT xmMuTargets
      LIBRARY DESTINATION lib
      ARCHIVE DESTINATION lib
      RUNTIME DESTINATION bin
      INCLUDES DESTINATION include)
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  if (BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt)
    add_subdirectory(test)
  endif ()
endfunction()

# xmmu_add_group(<name> <dep>...) — an INTERFACE "role tier" convenience target
# `<name>` (alias / exported as xmotion::<name>) that links the listed component
# libraries that exist. Lets a consumer depend on a tier instead of enumerating
# its libs, while the individual libraries remain available for minimal builds.
function(xmmu_add_group name)
  add_library(${name} INTERFACE)
  foreach (dep ${ARGN})
    if (TARGET ${dep})
      target_link_libraries(${name} INTERFACE ${dep})
    endif ()
  endforeach ()
  add_library(xmotion::${name} ALIAS ${name})
  install(TARGETS ${name} EXPORT xmMuTargets)
endfunction()

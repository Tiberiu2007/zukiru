# ZukiModule.cmake — the add_zuki_module() helper.
#
# Every engine module is created through this function so that warnings, the
# C++ standard, platform defines, the namespaced alias, include-dir layout,
# install rules and test wiring are IDENTICAL across the whole engine. Changing
# a policy here changes it everywhere.
#
# Usage:
#   add_zuki_module(render
#     PUBLIC_DEPS  core math assets      # deps used in this module's PUBLIC headers
#     PRIVATE_DEPS platform log          # deps used only in src/
#     BACKENDS     vulkan d3d12          # optional: compile src/backend/<b>/ per selection
#     SOURCES      src/extra.cpp         # optional: override source auto-globbing
#     HEADER_ONLY                        # optional: force an INTERFACE library
#   )
#
# Conventions enforced (see agents/PROJECT_STRUCTURE.md §2, §5):
#   * public headers under include/zuki/<name>/  ->  #include <zuki/<name>/x.hpp>
#   * private sources + headers under src/
#   * target `zuki_<name>`, alias `zuki::<name>`
#   * dep names without a `::` are resolved to `zuki::<dep>`.

include_guard(GLOBAL)

# Resolve a dependency token to a linkable target name.
function(_zuki_resolve_dep out dep)
  if(dep MATCHES "::")
    set(${out} "${dep}" PARENT_SCOPE)   # already namespaced/external (e.g. Catch2::)
  else()
    set(${out} "zuki::${dep}" PARENT_SCOPE)
  endif()
endfunction()

function(add_zuki_module name)
  set(options HEADER_ONLY)
  set(oneValue "")
  set(multiValue PUBLIC_DEPS PRIVATE_DEPS BACKENDS SOURCES)
  cmake_parse_arguments(ZK "${options}" "${oneValue}" "${multiValue}" ${ARGN})

  set(target "zuki_${name}")
  set(module_dir "${CMAKE_CURRENT_SOURCE_DIR}")

  # --- Gather sources ------------------------------------------------------
  if(ZK_SOURCES)
    set(sources ${ZK_SOURCES})
  else()
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS
      "${module_dir}/src/*.cpp" "${module_dir}/src/*.cxx")
  endif()

  # Backend selection: keep only src/backend/<b>/ for requested backends.
  if(ZK_BACKENDS AND sources)
    set(_kept "")
    foreach(src ${sources})
      set(_drop OFF)
      if(src MATCHES "/src/backend/([^/]+)/")
        set(_be "${CMAKE_MATCH_1}")
        if(NOT _be IN_LIST ZK_BACKENDS)
          set(_drop ON)
        endif()
      endif()
      if(NOT _drop)
        list(APPEND _kept "${src}")
      endif()
    endforeach()
    set(sources ${_kept})
  endif()

  # --- Create the target ---------------------------------------------------
  if(ZK_HEADER_ONLY OR NOT sources)
    add_library(${target} INTERFACE)
    set(vis INTERFACE)
    message(STATUS "Zuki module: ${name} (header-only)")
  else()
    add_library(${target} ${sources})
    set(vis PUBLIC)
    list(LENGTH sources _nsrc)
    message(STATUS "Zuki module: ${name} (library, ${_nsrc} source(s))")
  endif()
  add_library(zuki::${name} ALIAS ${target})

  # --- Include directories -------------------------------------------------
  # PUBLIC headers are installed & visible to consumers; src/ is private.
  if(vis STREQUAL "INTERFACE")
    target_include_directories(${target} INTERFACE
      $<BUILD_INTERFACE:${module_dir}/include>
      $<INSTALL_INTERFACE:include>)
  else()
    target_include_directories(${target}
      PUBLIC
        $<BUILD_INTERFACE:${module_dir}/include>
        $<INSTALL_INTERFACE:include>
      PRIVATE
        "${module_dir}/src")
  endif()

  # --- Standard, warnings, platform flags ----------------------------------
  target_compile_features(${target} ${vis} cxx_std_20)
  target_link_libraries(${target} ${vis} zuki::platform_flags)
  if(NOT vis STREQUAL "INTERFACE")
    target_link_libraries(${target} PRIVATE zuki::warnings)
  endif()

  # --- Dependencies --------------------------------------------------------
  foreach(dep ${ZK_PUBLIC_DEPS})
    _zuki_resolve_dep(_t "${dep}")
    target_link_libraries(${target} ${vis} ${_t})
  endforeach()
  foreach(dep ${ZK_PRIVATE_DEPS})
    _zuki_resolve_dep(_t "${dep}")
    if(vis STREQUAL "INTERFACE")
      message(FATAL_ERROR
        "Zuki module '${name}' is header-only but lists PRIVATE_DEPS '${dep}'."
        " Header-only modules can only have PUBLIC_DEPS.")
    endif()
    target_link_libraries(${target} PRIVATE ${_t})
  endforeach()

  # --- Install -------------------------------------------------------------
  install(TARGETS ${target} EXPORT ZukiTargets)
  if(EXISTS "${module_dir}/include")
    install(DIRECTORY "${module_dir}/include/" TYPE INCLUDE)
  endif()

  # --- Tests ---------------------------------------------------------------
  if(ZUKI_BUILD_TESTS)
    file(GLOB_RECURSE _tests CONFIGURE_DEPENDS "${module_dir}/tests/*.cpp")
    if(_tests)
      zuki_add_module_tests(${name} SOURCES ${_tests})
    endif()
  endif()
endfunction()

# Build a Catch2 test executable for a module and register it with CTest.
function(zuki_add_module_tests name)
  cmake_parse_arguments(T "" "" "SOURCES" ${ARGN})
  zuki_require_catch2()

  set(test_target "zuki_${name}_tests")
  add_executable(${test_target} ${T_SOURCES})
  target_link_libraries(${test_target} PRIVATE
    zuki::${name}
    zuki::warnings
    Catch2::Catch2WithMain)
  target_compile_features(${test_target} PRIVATE cxx_std_20)

  include(Catch)
  catch_discover_tests(${test_target}
    TEST_PREFIX "${name}."
    PROPERTIES LABELS "unit;${name}")
endfunction()

# Dependencies.cmake — third-party acquisition glue.
#
# Policy (see docs/adr/0001-package-manager.md):
#   * vcpkg (manifest mode, vcpkg.json) is the standard package manager. Its
#     toolchain is chainloaded automatically when VCPKG_ROOT is set — see
#     zukiru_bootstrap_vcpkg() below, which MUST run before project().
#   * Bootstrap-critical deps use FetchContent so a clean checkout builds with
#     no package manager installed. Keep this list short and documented.

include_guard(GLOBAL)

# Chainload the vcpkg toolchain if available. Call this from the root list
# BEFORE the project() command (toolchain files only take effect pre-project).
function(zukiru_bootstrap_vcpkg)
  if(DEFINED CMAKE_TOOLCHAIN_FILE)
    return()  # caller supplied a toolchain explicitly; respect it.
  endif()
  if(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    set(_tc "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    if(EXISTS "${_tc}")
      set(CMAKE_TOOLCHAIN_FILE "${_tc}" CACHE FILEPATH "vcpkg toolchain" FORCE)
      message(STATUS "Zukiru: using vcpkg toolchain at ${_tc}")
    else()
      message(WARNING "Zukiru: VCPKG_ROOT set but toolchain not found at ${_tc}")
    endif()
  endif()
endfunction()

# --- FetchContent-provided deps ------------------------------------------
include(FetchContent)

# Catch2 v3 — the unit-test framework (see ADR 0001). Prefer a system/vcpkg
# package if one is already discoverable; otherwise fetch a pinned tag.
function(zukiru_require_catch2)
  if(TARGET Catch2::Catch2WithMain)
    return()
  endif()
  find_package(Catch2 3 QUIET)
  if(Catch2_FOUND)
    message(STATUS "Zukiru: using system/vcpkg Catch2 ${Catch2_VERSION}")
    return()
  endif()

  message(STATUS "Zukiru: fetching Catch2 v3.7.1 via FetchContent")
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(Catch2)
  # Make Catch2's CTest integration helpers (catch_discover_tests) available.
  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
endfunction()

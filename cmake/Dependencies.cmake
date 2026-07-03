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
#
# Callable repeatedly and from any scope: it acquires Catch2 exactly once, then
# on every call re-applies the path to Catch2's CTest helpers (catch_discover_tests)
# to the *caller's* CMAKE_MODULE_PATH so a following `include(Catch)` works.
function(zukiru_require_catch2)
  if(NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 3 QUIET)
    if(Catch2_FOUND)
      message(STATUS "Zukiru: using system/vcpkg Catch2 ${Catch2_VERSION}")
    else()
      message(STATUS "Zukiru: fetching Catch2 v3.7.1 via FetchContent")
      FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE
      )
      FetchContent_MakeAvailable(Catch2)
    endif()
  endif()

  # Remember the extras dir (holds Catch.cmake) the first time we can see it,
  # then hand it to the caller on this and every later call.
  if(DEFINED catch2_SOURCE_DIR AND EXISTS "${catch2_SOURCE_DIR}/extras")
    set_property(GLOBAL PROPERTY ZUKIRU_CATCH_EXTRAS "${catch2_SOURCE_DIR}/extras")
  endif()
  get_property(_extras GLOBAL PROPERTY ZUKIRU_CATCH_EXTRAS)
  if(_extras)
    list(APPEND CMAKE_MODULE_PATH "${_extras}")
    list(REMOVE_DUPLICATES CMAKE_MODULE_PATH)
    set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
  endif()
endfunction()

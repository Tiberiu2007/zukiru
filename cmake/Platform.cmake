# Platform.cmake — OS/arch detection and normalized feature flags.
#
# Defines a set of cache-independent variables and an INTERFACE target
# `zuki_platform_flags` carrying platform #defines that every module inherits
# through add_zuki_module(). Include once from the root CMakeLists.txt.

include_guard(GLOBAL)

# --- OS detection ---------------------------------------------------------
set(ZUKI_OS_WINDOWS OFF)
set(ZUKI_OS_LINUX   OFF)
set(ZUKI_OS_MACOS   OFF)

if(WIN32)
  set(ZUKI_OS_WINDOWS ON)
  set(ZUKI_OS_NAME "windows")
elseif(APPLE)
  set(ZUKI_OS_MACOS ON)
  set(ZUKI_OS_NAME "macos")
elseif(UNIX)
  set(ZUKI_OS_LINUX ON)
  set(ZUKI_OS_NAME "linux")
else()
  message(FATAL_ERROR "Zuki: unsupported target OS '${CMAKE_SYSTEM_NAME}'")
endif()

# --- Architecture detection ----------------------------------------------
# CMAKE_SYSTEM_PROCESSOR is not fully portable; normalize the common cases.
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _zk_arch)
if(_zk_arch MATCHES "x86_64|amd64")
  set(ZUKI_ARCH "x64")
elseif(_zk_arch MATCHES "aarch64|arm64")
  set(ZUKI_ARCH "arm64")
elseif(_zk_arch MATCHES "i386|i686|x86")
  set(ZUKI_ARCH "x86")
else()
  set(ZUKI_ARCH "${_zk_arch}")
  message(STATUS "Zuki: unrecognized arch '${_zk_arch}', passing through")
endif()

message(STATUS "Zuki: target ${ZUKI_OS_NAME}/${ZUKI_ARCH}")

# --- Feature-flag INTERFACE target ---------------------------------------
# Modules link this transitively (via the helper) to pick up ZUKI_OS_* defines.
if(NOT TARGET zuki_platform_flags)
  add_library(zuki_platform_flags INTERFACE)
  add_library(zuki::platform_flags ALIAS zuki_platform_flags)

  target_compile_definitions(zuki_platform_flags INTERFACE
    $<$<BOOL:${ZUKI_OS_WINDOWS}>:ZUKI_OS_WINDOWS=1>
    $<$<BOOL:${ZUKI_OS_LINUX}>:ZUKI_OS_LINUX=1>
    $<$<BOOL:${ZUKI_OS_MACOS}>:ZUKI_OS_MACOS=1>
    $<$<CONFIG:Debug>:ZUKI_DEBUG=1>
  )

  # Sane per-OS baseline defines.
  if(ZUKI_OS_WINDOWS)
    target_compile_definitions(zuki_platform_flags INTERFACE
      WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  endif()
endif()

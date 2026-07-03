# Platform.cmake — OS/arch detection and normalized feature flags.
#
# Defines a set of cache-independent variables and an INTERFACE target
# `zukiru_platform_flags` carrying platform #defines that every module inherits
# through add_zukiru_module(). Include once from the root CMakeLists.txt.

include_guard(GLOBAL)

# --- OS detection ---------------------------------------------------------
set(ZUKIRU_OS_WINDOWS OFF)
set(ZUKIRU_OS_LINUX   OFF)
set(ZUKIRU_OS_MACOS   OFF)

if(WIN32)
  set(ZUKIRU_OS_WINDOWS ON)
  set(ZUKIRU_OS_NAME "windows")
elseif(APPLE)
  set(ZUKIRU_OS_MACOS ON)
  set(ZUKIRU_OS_NAME "macos")
elseif(UNIX)
  set(ZUKIRU_OS_LINUX ON)
  set(ZUKIRU_OS_NAME "linux")
else()
  message(FATAL_ERROR "Zukiru: unsupported target OS '${CMAKE_SYSTEM_NAME}'")
endif()

# --- Architecture detection ----------------------------------------------
# CMAKE_SYSTEM_PROCESSOR is not fully portable; normalize the common cases.
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _zk_arch)
if(_zk_arch MATCHES "x86_64|amd64")
  set(ZUKIRU_ARCH "x64")
elseif(_zk_arch MATCHES "aarch64|arm64")
  set(ZUKIRU_ARCH "arm64")
elseif(_zk_arch MATCHES "i386|i686|x86")
  set(ZUKIRU_ARCH "x86")
else()
  set(ZUKIRU_ARCH "${_zk_arch}")
  message(STATUS "Zukiru: unrecognized arch '${_zk_arch}', passing through")
endif()

message(STATUS "Zukiru: target ${ZUKIRU_OS_NAME}/${ZUKIRU_ARCH}")

# --- Feature-flag INTERFACE target ---------------------------------------
# Modules link this transitively (via the helper) to pick up ZUKIRU_OS_* defines.
if(NOT TARGET zukiru_platform_flags)
  add_library(zukiru_platform_flags INTERFACE)
  add_library(zukiru::platform_flags ALIAS zukiru_platform_flags)

  target_compile_definitions(zukiru_platform_flags INTERFACE
    $<$<BOOL:${ZUKIRU_OS_WINDOWS}>:ZUKIRU_OS_WINDOWS=1>
    $<$<BOOL:${ZUKIRU_OS_LINUX}>:ZUKIRU_OS_LINUX=1>
    $<$<BOOL:${ZUKIRU_OS_MACOS}>:ZUKIRU_OS_MACOS=1>
    $<$<CONFIG:Debug>:ZUKIRU_DEBUG=1>
  )

  # Sane per-OS baseline defines.
  if(ZUKIRU_OS_WINDOWS)
    target_compile_definitions(zukiru_platform_flags INTERFACE
      WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
  endif()
endif()

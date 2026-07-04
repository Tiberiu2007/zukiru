# platform — Layer 0. OS abstraction: timing, threads, dynamic libraries, file
# I/O, and native windowing (X11 + Wayland on Linux; Win32 / Cocoa are future
# work — see docs/adr/0005-native-windowing-backends.md). Depends on core.
# Namespace zukiru::platform.

# --- Windowing backend detection (Linux only for now) --------------------
set(_zk_window_backends "")
if(UNIX AND NOT APPLE)
  find_package(X11)
  find_package(PkgConfig)
  if(PkgConfig_FOUND)
    pkg_check_modules(WAYLAND_CLIENT wayland-client)
    pkg_check_modules(XKBCOMMON xkbcommon)
  endif()

  option(ZUKIRU_WINDOW_X11 "Build the native X11 window backend" "${X11_FOUND}")
  if(WAYLAND_CLIENT_FOUND AND XKBCOMMON_FOUND)
    set(_zk_wayland_default ON)
  else()
    set(_zk_wayland_default OFF)
  endif()
  option(ZUKIRU_WINDOW_WAYLAND "Build the native Wayland window backend" "${_zk_wayland_default}")

  if(ZUKIRU_WINDOW_X11)
    list(APPEND _zk_window_backends x11)
  endif()
  if(ZUKIRU_WINDOW_WAYLAND)
    list(APPEND _zk_window_backends wayland)
  endif()
endif()

add_zukiru_module(platform PUBLIC_DEPS core BACKENDS ${_zk_window_backends})

# System libraries: threads everywhere, and the dynamic-loader lib on POSIX.
find_package(Threads REQUIRED)
target_link_libraries(zukiru_platform PUBLIC Threads::Threads)
if(NOT WIN32)
  target_link_libraries(zukiru_platform PRIVATE ${CMAKE_DL_LIBS})
endif()

# --- X11 backend ---------------------------------------------------------
if(ZUKIRU_WINDOW_X11)
  target_link_libraries(zukiru_platform PRIVATE X11::X11)
  target_compile_definitions(zukiru_platform PRIVATE ZUKIRU_WINDOW_X11=1)
  message(STATUS "Zukiru platform: X11 window backend enabled")
endif()

# --- Wayland backend -----------------------------------------------------
# Generates the xdg-shell client protocol with wayland-scanner and compiles it in
# its own library (no zukiru::warnings) so -Werror never policies generated C.
if(ZUKIRU_WINDOW_WAYLAND)
  enable_language(C)  # the generated xdg-shell protocol code is C
  find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
  pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)

  set(_xdg_xml "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
  set(_wl_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/wayland-generated")
  set(_xdg_header "${_wl_gen_dir}/xdg-shell-client-protocol.h")
  set(_xdg_code "${_wl_gen_dir}/xdg-shell-protocol.c")
  file(MAKE_DIRECTORY "${_wl_gen_dir}")

  add_custom_command(
    OUTPUT "${_xdg_header}"
    COMMAND "${WAYLAND_SCANNER}" client-header "${_xdg_xml}" "${_xdg_header}"
    DEPENDS "${_xdg_xml}"
    VERBATIM)
  add_custom_command(
    OUTPUT "${_xdg_code}"
    COMMAND "${WAYLAND_SCANNER}" private-code "${_xdg_xml}" "${_xdg_code}"
    DEPENDS "${_xdg_xml}"
    VERBATIM)

  # The generated code is C; mark it so before it exists on disk.
  set_source_files_properties("${_xdg_code}" PROPERTIES LANGUAGE C GENERATED TRUE)
  set_source_files_properties("${_xdg_header}" PROPERTIES GENERATED TRUE)
  add_library(zukiru_wayland_protocols STATIC "${_xdg_code}" "${_xdg_header}")
  target_include_directories(zukiru_wayland_protocols PUBLIC "${_wl_gen_dir}"
                             ${WAYLAND_CLIENT_INCLUDE_DIRS})
  set_target_properties(zukiru_wayland_protocols PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    LINKER_LANGUAGE C)

  target_link_libraries(zukiru_platform PRIVATE
    zukiru_wayland_protocols
    ${WAYLAND_CLIENT_LIBRARIES}
    ${XKBCOMMON_LIBRARIES})
  target_include_directories(zukiru_platform PRIVATE
    "${_wl_gen_dir}"
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${XKBCOMMON_INCLUDE_DIRS})
  target_compile_definitions(zukiru_platform PRIVATE ZUKIRU_WINDOW_WAYLAND=1)
  message(STATUS "Zukiru platform: Wayland window backend enabled")
endif()

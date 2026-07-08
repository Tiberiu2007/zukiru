# Dependencies.cmake — third-party acquisition glue.
#
# Policy (see docs/adr/0001-package-manager.md):
#   * vcpkg (manifest mode, vcpkg.json) is the standard package manager. Its
#     toolchain is chainloaded automatically when VCPKG_ROOT is set — see
#     zuki_bootstrap_vcpkg() below, which MUST run before project().
#   * Bootstrap-critical deps use FetchContent so a clean checkout builds with
#     no package manager installed. Keep this list short and documented.

include_guard(GLOBAL)

# Chainload the vcpkg toolchain if available. Call this from the root list
# BEFORE the project() command (toolchain files only take effect pre-project).
function(zuki_bootstrap_vcpkg)
  if(DEFINED CMAKE_TOOLCHAIN_FILE)
    return()  # caller supplied a toolchain explicitly; respect it.
  endif()
  if(DEFINED ENV{VCPKG_ROOT} AND NOT "$ENV{VCPKG_ROOT}" STREQUAL "")
    set(_tc "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    if(EXISTS "${_tc}")
      set(CMAKE_TOOLCHAIN_FILE "${_tc}" CACHE FILEPATH "vcpkg toolchain" FORCE)
      message(STATUS "Zuki: using vcpkg toolchain at ${_tc}")
    else()
      message(WARNING "Zuki: VCPKG_ROOT set but toolchain not found at ${_tc}")
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
function(zuki_require_catch2)
  if(NOT TARGET Catch2::Catch2WithMain)
    find_package(Catch2 3 QUIET)
    if(Catch2_FOUND)
      message(STATUS "Zuki: using system/vcpkg Catch2 ${Catch2_VERSION}")
    else()
      message(STATUS "Zuki: fetching Catch2 v3.7.1 via FetchContent")
      FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE
      )
      FetchContent_MakeAvailable(Catch2)
    endif()
  endif()

  # Treat Catch2's headers as SYSTEM so our strict -Werror policy never fires on
  # third-party code (e.g. GCC's -Wnull-dereference false positives inside its
  # own std::string comparison helpers). Requires CMake 3.25+ for the property.
  if(NOT CMAKE_VERSION VERSION_LESS 3.25)
    foreach(_catchTarget Catch2 Catch2WithMain)
      if(TARGET ${_catchTarget})
        set_target_properties(${_catchTarget} PROPERTIES SYSTEM TRUE)
      endif()
    endforeach()
  endif()

  # Remember the extras dir (holds Catch.cmake) the first time we can see it,
  # then hand it to the caller on this and every later call.
  if(DEFINED catch2_SOURCE_DIR AND EXISTS "${catch2_SOURCE_DIR}/extras")
    set_property(GLOBAL PROPERTY ZUKI_CATCH_EXTRAS "${catch2_SOURCE_DIR}/extras")
  endif()
  get_property(_extras GLOBAL PROPERTY ZUKI_CATCH_EXTRAS)
  if(_extras)
    list(APPEND CMAKE_MODULE_PATH "${_extras}")
    list(REMOVE_DUPLICATES CMAKE_MODULE_PATH)
    set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
  endif()
endfunction()

# Vulkan for the render backend. Provides an INTERFACE target `zuki_vulkan`
# carrying the Vulkan headers + the loader. Headers come from a system SDK if one
# is installed, else Vulkan-Headers via FetchContent (a clean box only ships the
# runtime loader `libvulkan.so.1`, not the dev headers). Idempotent.
function(zuki_require_vulkan)
  if(TARGET zuki_vulkan)
    return()
  endif()

  # Headers: prefer a discoverable system/SDK copy, else fetch a pinned set.
  find_path(ZUKI_VULKAN_INCLUDE_DIR vulkan/vulkan.h)
  if(ZUKI_VULKAN_INCLUDE_DIR)
    message(STATUS "Zuki: using system Vulkan headers at ${ZUKI_VULKAN_INCLUDE_DIR}")
  else()
    message(STATUS "Zuki: fetching Vulkan-Headers via FetchContent")
    # Don't build Vulkan-Headers' optional C++20 module target: we use the classic
    # C headers, and the module hits a GCC ICE under sanitizer flags.
    set(VULKAN_HEADERS_ENABLE_MODULE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(VulkanHeaders
      GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
      GIT_TAG        v1.3.296
      GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(VulkanHeaders)  # defines target Vulkan::Headers
  endif()

  # Loader: many distros ship only the versioned soname (libvulkan.so.1) without
  # the `.so` dev symlink, so teach find_library to accept it.
  set(_saved_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
  list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES .so.1)
  find_library(ZUKI_VULKAN_LOADER NAMES vulkan vulkan-1)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ${_saved_suffixes})
  if(NOT ZUKI_VULKAN_LOADER)
    message(FATAL_ERROR "Zuki: Vulkan loader (libvulkan) not found")
  endif()
  message(STATUS "Zuki: Vulkan loader at ${ZUKI_VULKAN_LOADER}")

  add_library(zuki_vulkan INTERFACE)
  target_link_libraries(zuki_vulkan INTERFACE "${ZUKI_VULKAN_LOADER}")
  # Add the headers as SYSTEM so our -Werror set never fires on the Vulkan
  # headers' own C-style-cast macros (VK_MAKE_VERSION etc.) at expansion sites.
  if(TARGET Vulkan::Headers)
    get_target_property(_vk_inc Vulkan::Headers INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(zuki_vulkan SYSTEM INTERFACE "${_vk_inc}")
  else()
    target_include_directories(zuki_vulkan SYSTEM INTERFACE "${ZUKI_VULKAN_INCLUDE_DIR}")
  endif()
endfunction()

# glslang for the offline shader compiler (GLSL -> SPIR-V). No system SPIR-V
# toolchain is assumed, so glslang is fetched and built. Provides an INTERFACE
# target `zuki_glslang` (its headers marked SYSTEM). Only used by tools/, so it
# is not fetched unless a tool asks for it. Idempotent.
function(zuki_require_glslang)
  if(TARGET zuki_glslang)
    return()
  endif()

  message(STATUS "Zuki: fetching glslang via FetchContent (shader compiler)")
  # Trim glslang's build to just the libraries we link.
  set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
  set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
  set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
  set(ENABLE_OPT OFF CACHE BOOL "" FORCE)          # skip SPIRV-Tools optimizer dep
  set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
  set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
  # Emit typeinfo for glslang's polymorphic classes (TShader/TProgram). Without
  # it, our TU's references to their typeinfo go undefined at link time under some
  # optimization/sanitizer settings.
  set(ENABLE_RTTI ON CACHE BOOL "" FORCE)

  FetchContent_Declare(glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG        14.3.0
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(glslang)

  add_library(zuki_glslang INTERFACE)
  target_link_libraries(zuki_glslang INTERFACE glslang SPIRV glslang-default-resource-limits)
  # Mark glslang's headers SYSTEM so our strict -Werror set never polices them.
  foreach(_t glslang SPIRV glslang-default-resource-limits)
    if(TARGET ${_t})
      get_target_property(_inc ${_t} INTERFACE_INCLUDE_DIRECTORIES)
      if(_inc)
        target_include_directories(zuki_glslang SYSTEM INTERFACE ${_inc})
      endif()
    endif()
  endforeach()
endfunction()

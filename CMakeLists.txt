# Zukiru — root build.
#
# The engine is a set of layered libraries (engine/modules/*), consumed by the
# editor, tools, runtime, games and tests. See agents/PROJECT_STRUCTURE.md.

cmake_minimum_required(VERSION 3.24)

# vcpkg toolchain must be chainloaded BEFORE project(). See ADR 0001.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Dependencies)
zukiru_bootstrap_vcpkg()

project(Zukiru
  VERSION 0.1.0
  DESCRIPTION "Zukiru — a modular, data-oriented C++20 game engine"
  LANGUAGES CXX)

# --- Guard against in-source builds --------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR
    "Zukiru: in-source builds are not allowed. Configure with a preset, e.g.\n"
    "  cmake --preset debug")
endif()

# --- Global standard / output layout -------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

# Default to a build type for single-config generators.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

# Only Zukiru's own targets should be treated this strictly; keep the export
# of compile_commands.json on for tooling (clang-tidy, editors).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Options -------------------------------------------------------------
option(ZUKIRU_BUILD_EDITOR     "Build the editor application" OFF)
option(ZUKIRU_BUILD_TESTS      "Build unit + integration tests" ON)
option(ZUKIRU_BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(ZUKIRU_BUILD_TOOLS      "Build offline tools (asset cooker, etc.)" OFF)
option(ZUKIRU_BUILD_GAMES      "Build sample games" OFF)
option(ZUKIRU_BUILD_SHARED     "Build engine modules as shared libraries" OFF)
set(ZUKIRU_RENDER_BACKEND "vulkan" CACHE STRING "Primary render backend")
set_property(CACHE ZUKIRU_RENDER_BACKEND PROPERTY STRINGS vulkan d3d12 metal)

set(BUILD_SHARED_LIBS ${ZUKIRU_BUILD_SHARED})

# --- Shared CMake infrastructure -----------------------------------------
include(Platform)          # OS/arch detection + zukiru::platform_flags
include(CompilerWarnings)  # zukiru::warnings
include(ZukiruModule)      # add_zukiru_module()

if(ZUKIRU_BUILD_TESTS)
  enable_testing()
endif()

# --- Areas ---------------------------------------------------------------
# Each area is optional and only added if it exists, so the tree can grow
# incrementally without breaking configure.
add_subdirectory(engine)

if(ZUKIRU_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
  add_subdirectory(tests)
endif()
if(ZUKIRU_BUILD_TOOLS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/CMakeLists.txt")
  add_subdirectory(tools)
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/runtime/CMakeLists.txt")
  add_subdirectory(runtime)
endif()
if(ZUKIRU_BUILD_GAMES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/games/CMakeLists.txt")
  add_subdirectory(games)
endif()
if(ZUKIRU_BUILD_EDITOR AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/editor/CMakeLists.txt")
  add_subdirectory(editor)
endif()
if(ZUKIRU_BUILD_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/CMakeLists.txt")
  add_subdirectory(benchmarks)
endif()

message(STATUS "Zukiru ${PROJECT_VERSION} configured "
  "(editor=${ZUKIRU_BUILD_EDITOR} tests=${ZUKIRU_BUILD_TESTS} "
  "backend=${ZUKIRU_RENDER_BACKEND} shared=${ZUKIRU_BUILD_SHARED})")

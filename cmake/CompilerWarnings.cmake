# CompilerWarnings.cmake — a shared warning set for every Zukiru module.
#
# Exposes an INTERFACE target `zukiru::warnings` that the module helper links
# PRIVATE-ly onto each module. Keeping this in one place means "turn on a new
# warning" is a one-line change that applies engine-wide.
#
# Controlled by the ZUKIRU_WARNINGS_AS_ERRORS option (defined in the root list).

include_guard(GLOBAL)

if(TARGET zukiru_warnings)
  return()
endif()

add_library(zukiru_warnings INTERFACE)
add_library(zukiru::warnings ALIAS zukiru_warnings)

option(ZUKIRU_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(_zk_msvc_warnings
  /W4
  /permissive-        # stricter standard conformance
  /w14242 /w14254 /w14263 /w14265 /w14287 /w14296 /w14311
  /w14545 /w14546 /w14547 /w14549 /w14555
  /w14640             # thread-unsafe static init
  /w14826 /w14905 /w14906 /w14928
)

set(_zk_gcc_clang_warnings
  -Wall
  -Wextra
  -Wpedantic
  -Wshadow
  -Wnon-virtual-dtor
  -Wold-style-cast
  -Wcast-align
  -Wunused
  -Woverloaded-virtual
  -Wconversion
  -Wsign-conversion
  -Wnull-dereference
  -Wdouble-promotion
  -Wformat=2
  -Wimplicit-fallthrough
)

set(_zk_gcc_only_warnings
  -Wmisleading-indentation
  -Wduplicated-cond
  -Wduplicated-branches
  -Wlogical-op
  -Wuseless-cast
)

if(MSVC)
  set(_zk_warnings ${_zk_msvc_warnings})
  if(ZUKIRU_WARNINGS_AS_ERRORS)
    list(APPEND _zk_warnings /WX)
  endif()
else()
  set(_zk_warnings ${_zk_gcc_clang_warnings})
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND _zk_warnings ${_zk_gcc_only_warnings})
  endif()
  if(ZUKIRU_WARNINGS_AS_ERRORS)
    list(APPEND _zk_warnings -Werror)
  endif()
endif()

target_compile_options(zukiru_warnings INTERFACE
  $<$<COMPILE_LANGUAGE:CXX>:${_zk_warnings}>)

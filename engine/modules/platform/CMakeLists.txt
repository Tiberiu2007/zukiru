# platform — Layer 0. OS abstraction: timing, threads, dynamic libraries, file
# I/O, and the (interface-only, backend-deferred) windowing surface. Depends on
# core. Namespace zukiru::platform.
add_zukiru_module(platform PUBLIC_DEPS core)

# System libraries: threads everywhere, and the dynamic-loader lib on POSIX.
find_package(Threads REQUIRED)
target_link_libraries(zukiru_platform PUBLIC Threads::Threads)
if(NOT WIN32)
  target_link_libraries(zukiru_platform PRIVATE ${CMAKE_DL_LIBS})
endif()

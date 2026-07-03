# tests/ — cross-module integration tests and the build-pipeline smoke test.
# (Per-module unit tests live inside each engine module's tests/ folder and are
#  wired automatically by add_zukiru_module().)

zukiru_require_catch2()
include(Catch)

# --- Pipeline smoke test --------------------------------------------------
# The single "trivial passing test" that proves the whole toolchain works:
# CMake configure -> Catch2 acquisition -> compile -> link -> CTest discovery.
add_executable(zukiru_smoke_test smoke_test.cpp)
target_link_libraries(zukiru_smoke_test PRIVATE
  zukiru::warnings
  zukiru::platform_flags
  Catch2::Catch2WithMain)
target_compile_features(zukiru_smoke_test PRIVATE cxx_std_20)

catch_discover_tests(zukiru_smoke_test
  TEST_PREFIX "smoke."
  PROPERTIES LABELS "smoke")

# --- Integration tests ----------------------------------------------------
# Populated as modules land; kept optional so the tree configures while empty.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration/CMakeLists.txt")
  add_subdirectory(integration)
endif()

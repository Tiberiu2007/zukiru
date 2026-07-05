# shader_compiler — offline GLSL -> SPIR-V. A small library (zukiru::shaderc) over
# glslang plus a CLI front-end (zukiru-shaderc). glslang is a private impl detail:
# the library's public header exposes no glslang types, so SPIR-V can be cooked
# offline and consumed without the compiler at runtime.

zukiru_require_glslang()

# --- Library -------------------------------------------------------------
add_library(zukiru_shaderc STATIC src/shader_compiler.cpp)
add_library(zukiru::shaderc ALIAS zukiru_shaderc)
target_include_directories(zukiru_shaderc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(zukiru_shaderc PUBLIC cxx_std_20)
target_link_libraries(zukiru_shaderc
  PUBLIC  zukiru::core
  PRIVATE zukiru_glslang zukiru::warnings zukiru::platform_flags)

# --- CLI -----------------------------------------------------------------
add_executable(zukiru-shaderc src/main.cpp)
target_link_libraries(zukiru-shaderc PRIVATE zukiru::shaderc zukiru::warnings zukiru::platform_flags)

# --- Tests ---------------------------------------------------------------
if(ZUKIRU_BUILD_TESTS)
  zukiru_require_catch2()
  add_executable(zukiru_shaderc_tests tests/shader_compiler_test.cpp)
  target_link_libraries(zukiru_shaderc_tests
    PRIVATE zukiru::shaderc zukiru::warnings Catch2::Catch2WithMain)
  target_compile_features(zukiru_shaderc_tests PRIVATE cxx_std_20)
  include(Catch)
  catch_discover_tests(zukiru_shaderc_tests
    TEST_PREFIX "shaderc."
    PROPERTIES LABELS "unit;shaderc")
endif()

# shader_compiler — offline GLSL -> SPIR-V. A small library (zuki::shaderc) over
# glslang plus a CLI front-end (zuki-shaderc). glslang is a private impl detail:
# the library's public header exposes no glslang types, so SPIR-V can be cooked
# offline and consumed without the compiler at runtime.

zuki_require_glslang()

# --- Library -------------------------------------------------------------
add_library(zuki_shaderc STATIC src/shader_compiler.cpp)
add_library(zuki::shaderc ALIAS zuki_shaderc)
target_include_directories(zuki_shaderc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(zuki_shaderc PUBLIC cxx_std_20)
target_link_libraries(zuki_shaderc
  PUBLIC  zuki::core
  PRIVATE zuki_glslang zuki::warnings zuki::platform_flags)

# --- CLI -----------------------------------------------------------------
add_executable(zuki-shaderc src/main.cpp)
target_link_libraries(zuki-shaderc PRIVATE zuki::shaderc zuki::warnings zuki::platform_flags)

# --- Tests ---------------------------------------------------------------
if(ZUKI_BUILD_TESTS)
  zuki_require_catch2()
  add_executable(zuki_shaderc_tests tests/shader_compiler_test.cpp)
  target_link_libraries(zuki_shaderc_tests
    PRIVATE zuki::shaderc zuki::warnings Catch2::Catch2WithMain)
  target_compile_features(zuki_shaderc_tests PRIVATE cxx_std_20)
  include(Catch)
  catch_discover_tests(zuki_shaderc_tests
    TEST_PREFIX "shaderc."
    PROPERTIES LABELS "unit;shaderc")
endif()

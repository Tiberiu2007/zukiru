# sandbox — a minimal playable demo: a hierarchy of spinning textured cubes with an
# orbit camera. Exercises app + scene + ecs + render + renderer (MeshRenderer entities
# drawn by renderMeshes; ring-buffered per-frame uniform + push constants + depth) end
# to end on the real GPU.
#
# Shaders are cooked offline by zuki-shaderc and embedded (src/sandbox_shaders.hpp),
# so the demo needs no shader compiler at build or run time.
add_executable(zuki_sandbox src/main.cpp)

target_link_libraries(zuki_sandbox PRIVATE
  zuki::app
  zuki::render
  zuki::renderer
  zuki::scene
  zuki::ecs
  zuki::input
  zuki::platform
  zuki::math
  zuki::log
  zuki::core
  zuki::warnings
  zuki::platform_flags)

target_include_directories(zuki_sandbox PRIVATE src)
target_compile_features(zuki_sandbox PRIVATE cxx_std_20)

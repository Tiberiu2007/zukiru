# sandbox — a minimal playable demo: a hierarchy of spinning textured cubes with an
# orbit camera. Exercises app + scene + ecs + render + renderer (MeshRenderer entities
# drawn by renderMeshes; ring-buffered per-frame uniform + push constants + depth) end
# to end on the real GPU.
#
# Shaders are cooked offline by zukiru-shaderc and embedded (src/sandbox_shaders.hpp),
# so the demo needs no shader compiler at build or run time.
add_executable(zukiru_sandbox src/main.cpp)

target_link_libraries(zukiru_sandbox PRIVATE
  zukiru::app
  zukiru::render
  zukiru::renderer
  zukiru::scene
  zukiru::ecs
  zukiru::input
  zukiru::platform
  zukiru::math
  zukiru::log
  zukiru::core
  zukiru::warnings
  zukiru::platform_flags)

target_include_directories(zukiru_sandbox PRIVATE src)
target_compile_features(zukiru_sandbox PRIVATE cxx_std_20)

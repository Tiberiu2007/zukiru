# render — Layer 2 (core subsystems). RHI abstraction + a Vulkan backend that
# brings up a swapchain and clears/presents each frame, plus cameras. Public API
# (rhi.hpp/camera.hpp) is backend-agnostic; Vulkan is a private dependency. See
# docs/adr/0006-render-architecture.md. Depends on core, math, platform.
# Namespace zukiru::render.

zukiru_require_vulkan()  # provides the zukiru_vulkan interface target

# The Vulkan surface create-info structs need the native window headers (Xlib /
# wayland-client) for their types. Surface *functions* come from the Vulkan
# loader, so we take the include dirs but do NOT link libX11 / libwayland.
if(UNIX AND NOT APPLE)
  find_package(X11)
  find_package(PkgConfig)
  if(PkgConfig_FOUND)
    pkg_check_modules(WAYLAND_CLIENT wayland-client)
  endif()
endif()

add_zukiru_module(render PUBLIC_DEPS core math platform)

target_link_libraries(zukiru_render PRIVATE zukiru_vulkan)

if(X11_FOUND)
  target_compile_definitions(zukiru_render PRIVATE ZUKIRU_RENDER_VK_XLIB=1)
  target_include_directories(zukiru_render SYSTEM PRIVATE ${X11_INCLUDE_DIR})
endif()
if(WAYLAND_CLIENT_FOUND)
  target_compile_definitions(zukiru_render PRIVATE ZUKIRU_RENDER_VK_WAYLAND=1)
  target_include_directories(zukiru_render SYSTEM PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIRS})
endif()

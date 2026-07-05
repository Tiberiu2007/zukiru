# render

**Layer 2 — core subsystems.** An RHI (render hardware interface) abstraction with
a **Vulkan** backend, plus cameras. Game/scene code talks to the backend-agnostic
RHI, never to Vulkan directly, so a second backend (D3D12/Metal) can drop in later.

This first cut brings up the full Vulkan chain (instance → surface → device →
swapchain → render pass → present) and **clears the screen each frame**. Command
recording, resources, pipelines, shaders and the first triangle come with the
Milestone-4 shader toolchain — they extend the RHI without breaking it. See
[ADR 0006](../../../docs/adr/0006-render-architecture.md). Namespace `zukiru::render`.

## Rendering a frame

```cpp
#include <zukiru/render/render.hpp>

auto window = platform::createWindow({.title = "Zukiru"});
auto device = render::createDevice(*window.value());   // picks Vulkan
device.value()->setClearColor({0.1f, 0.1f, 0.15f, 1.0f});

while (!window.value()->shouldClose()) {
    window.value()->pollEvents();
    if (!device.value()->beginFrame()) {               // swapchain out of date?
        auto e = window.value()->extent();
        device.value()->resize(e.width, e.height);
        continue;
    }
    device.value()->endFrame();                        // submit + present
}
device.value()->waitIdle();
```

`Device` is the whole public surface for now: `beginFrame` / `endFrame`,
`setClearColor`, `resize`, `waitIdle`, `backend()`, `deviceName()`. It is
**backend-agnostic** — no Vulkan types leak into the public headers.

## Cameras

Pure `math`, backend-independent, header-only:

```cpp
render::Camera cam;
cam.setPerspective(math::radians(60.0f), 16.0f / 9.0f, 0.1f, 1000.0f);
cam.lookAt({0, 2, 5}, {0, 0, 0}, math::Vec3::unitY());
math::Mat4 vp = cam.viewProjection();   // projection * view
```

`setTransform(pos, rot)` places the camera by a rigid transform (the view is its
inverse); `setOrthographic` / `setProjection` / `setView` cover the rest.

## Vulkan backend (private)

`src/vulkan/` implements the RHI: instance (+ optional validation), a surface from
the window's native handle (Xlib or Wayland per `platform::Window::nativeBackend()`),
physical-device selection (graphics+present+swapchain, prefers a discrete GPU),
logical device, swapchain (format/present-mode/extent), a single-attachment
render pass (`loadOp=CLEAR`), framebuffers, command buffers, and per-frame sync
(2 frames in flight). Out-of-date/suboptimal swapchains and `resize()` trigger
recreation.

- **Vulkan is a private dependency**: headers come from a system SDK or, failing
  that, **Vulkan-Headers via FetchContent** (a clean box ships only
  `libvulkan.so.1`). Public consumers see only the RHI.
- The X11 surface uses its **own** `Display` connection, closed while the instance
  is still alive — this sidesteps an NVIDIA driver crash where `XCloseDisplay`
  invokes a hook into an already-unloaded ICD.

## Scope

Clear-screen MVP + cameras. **Deferred** (additive, no API break): command/resource/
pipeline RHI, shaders + first triangle, render graph, materials, depth/MSAA, and
additional backends (D3D12/Metal). Wayland surface support compiles but is
runtime-validated only on a Wayland session (see ADR 0005).

## Tests

```bash
ctest --preset debug -R '^render\.'          # camera + RHI (CI-safe)
zukiru_render_tests "[.gpu]"                 # real GPU: bring-up + clear + present
```

Camera math and RHI value types are covered by ordinary unit tests. The hidden
`[.gpu]` test opens a real window and renders/presents 20 frames (with a resize)
on the actual device — verified on an NVIDIA RTX 3060 and clean under ASan.

## Dependencies

`core`, `math`, `platform` (Layer 2); Vulkan (private). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).

# render

**Layer 2 — core subsystems.** An RHI (render hardware interface) abstraction with
a **Vulkan** backend, plus cameras. Game/scene code talks to the backend-agnostic
RHI, never to Vulkan directly, so a second backend (D3D12/Metal) can drop in later.

It brings up the full Vulkan chain (instance → surface → device → swapchain →
render pass → present) and exposes **GPU buffers**, **SPIR-V graphics pipelines**,
and **per-frame command recording** — enough to draw your own geometry. See
[ADR 0006](../../../docs/adr/0006-render-architecture.md). Namespace `zukiru::render`.

## Drawing your own geometry

```cpp
#include <zukiru/render/render.hpp>

auto device = render::createDevice(*window).value();

// Upload a vertex buffer and build a pipeline that reads it.
auto vbo = device->createBuffer(render::BufferKind::Vertex, verts, sizeof(verts));
render::PipelineDesc desc;
desc.vertexSpirv   = vertSpirv;    // std::span<const u32> — cooked by zukiru-shaderc
desc.fragmentSpirv = fragSpirv;
desc.vertexLayout.stride = sizeof(Vertex);
desc.vertexLayout.attributes = {
    {.location = 0, .format = render::VertexFormat::Float32x2, .offset = 0},
    {.location = 1, .format = render::VertexFormat::Float32x3, .offset = 8},
};
auto pipeline = device->createPipeline(desc).value();

while (running) {
    if (!device->beginFrame()) { device->resize(w, h); continue; }
    device->bindPipeline(pipeline);
    device->bindVertexBuffer(vbo);
    device->draw(3);
    device->endFrame();
}
```

`Device` is **backend-agnostic** — no Vulkan types leak into the public headers.
It exposes: resources (`createBuffer`/`createPipeline` + `destroy*`, indexed by
opaque `BufferHandle`/`PipelineHandle`); the frame (`beginFrame` clears + opens
the render pass, `endFrame` submits + presents); recording, valid between the two
(`bindPipeline`, `bindVertexBuffer`, `bindIndexBuffer`, `draw`, `drawIndexed`);
plus `setClearColor`, `resize`, `waitIdle`, `backend()`, `deviceName()`.

Pipelines take **SPIR-V** (`std::span<const u32>`) — produce it offline with
[`zukiru-shaderc`](../../../tools/shader_compiler) and embed or load it. Viewport/
scissor are dynamic, so pipelines survive window resizes without a rebuild.

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
(2 frames in flight). Buffers are host-visible/coherent (map + memcpy); pipelines
are built from the `PipelineDesc` (SPIR-V modules + vertex layout + topology) with
dynamic viewport/scissor. Out-of-date/suboptimal swapchains and `resize()` trigger
recreation; pipelines are unaffected.

- **Vulkan is a private dependency**: headers come from a system SDK or, failing
  that, **Vulkan-Headers via FetchContent** (a clean box ships only
  `libvulkan.so.1`). Public consumers see only the RHI.
- The X11 surface uses its **own** `Display` connection, closed while the instance
  is still alive — this sidesteps an NVIDIA driver crash where `XCloseDisplay`
  invokes a hook into an already-unloaded ICD.

## Scope

Buffers, SPIR-V pipelines, command recording (draw / draw-indexed), cameras.
**Deferred** (additive, no API break): staging uploads + a real GPU allocator
(buffers are host-visible today), textures/samplers, uniform/descriptor sets and
push constants, depth/MSAA, a render graph and materials, and additional backends
(D3D12/Metal). Wayland surface support compiles but is runtime-validated only on a
Wayland session (see ADR 0005).

## Tests

```bash
ctest --preset debug -R '^render\.'          # camera + RHI value types (CI-safe)
zukiru_render_tests "[.gpu]"                 # real GPU: upload + pipeline + draw
```

Camera math and RHI value types are covered by ordinary unit tests. The hidden
`[.gpu]` test opens a real window, uploads a vertex buffer, builds a pipeline from
SPIR-V, and draws user geometry for 20 frames (with a resize) — plus an
invalid-SPIR-V rejection case. Verified on an NVIDIA RTX 3060, clean under ASan.
The demo shaders live in [`tests/shaders/`](tests/shaders) (GLSL) and are embedded
as SPIR-V by `zukiru-shaderc` — so the render module itself needs no shader
compiler at build or run time.

## Dependencies

`core`, `math`, `platform` (Layer 2); Vulkan (private). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).

# render

**Layer 2 — core subsystems.** An RHI (render hardware interface) abstraction with
a **Vulkan** backend, plus cameras. Game/scene code talks to the backend-agnostic
RHI, never to Vulkan directly, so a second backend (D3D12/Metal) can drop in later.

It brings up the full Vulkan chain (instance → surface → device → swapchain →
render pass → present) and exposes **GPU buffers** (vertex/index/uniform),
**textures**, **SPIR-V graphics pipelines**, **bind groups** (uniform + texture
descriptor sets), and **per-frame command recording** — enough to draw your own
textured, shader-parameterized geometry. See
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
It exposes: resources (`createBuffer` (Vertex/Index/Uniform) + `updateBuffer`,
`createTexture` (RGBA8), `createPipeline`, `createBindGroup`, and `destroy*` —
opaque `BufferHandle`/`TextureHandle`/`PipelineHandle`/`BindGroupHandle`); the
frame (`beginFrame` clears + opens the render pass, `endFrame` submits + presents);
recording between the two (`bindPipeline`, `bindBindGroup`, `bindVertexBuffer`,
`bindIndexBuffer`, `draw`, `drawIndexed`); plus `setClearColor`, `resize`,
`waitIdle`, `backend()`, `deviceName()`.

## Uniforms and textures

A pipeline declares its resource slots in `PipelineDesc::bindings` (descriptor
set 0): `BindingType::UniformBuffer` or `Texture`. A **bind group** supplies the
concrete resources for those slots and is bound during recording:

```cpp
desc.bindings = {render::BindingType::UniformBuffer, render::BindingType::Texture};
auto pipeline = device->createPipeline(desc).value();

auto ubo = device->createBuffer(render::BufferKind::Uniform, &mvp, sizeof(mvp));
auto tex = device->createTexture(w, h, rgbaPixels);
render::BindGroupEntry entries[] = {
    {.binding = 0, .buffer = ubo},
    {.binding = 1, .texture = tex},
};
auto group = device->createBindGroup(pipeline, entries).value();

// per frame: device->bindPipeline(pipeline); device->bindBindGroup(group); ...
device->updateBuffer(ubo, &mvp, sizeof(mvp));  // e.g. a new camera matrix
```

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
(2 frames in flight). Buffers are host-visible/coherent (map + memcpy); textures
upload through a staging buffer with image-layout transitions (single-time command
submits) and get a view + linear sampler. Pipelines are built from the
`PipelineDesc` (SPIR-V + vertex layout + topology + a descriptor-set-0 layout from
`bindings`) with dynamic viewport/scissor; bind groups are descriptor sets from a
shared pool. Out-of-date/suboptimal swapchains and `resize()` trigger recreation;
pipelines and resources are unaffected.

- **Vulkan is a private dependency**: headers come from a system SDK or, failing
  that, **Vulkan-Headers via FetchContent** (a clean box ships only
  `libvulkan.so.1`). Public consumers see only the RHI.
- The X11 surface uses its **own** `Display` connection, closed while the instance
  is still alive — this sidesteps an NVIDIA driver crash where `XCloseDisplay`
  invokes a hook into an already-unloaded ICD.

## Scope

Buffers (vertex/index/uniform), RGBA8 textures, SPIR-V pipelines with uniform +
texture bind groups, command recording (draw / draw-indexed), cameras.
**Deferred** (additive, no API break): staging/DEVICE_LOCAL vertex buffers + a real
GPU allocator (vertex/uniform buffers are host-visible today), per-frame uniform
ring buffers (`updateBuffer` must not race an in-flight frame), push constants,
mipmaps, depth/MSAA, a render graph and materials, and additional backends
(D3D12/Metal). Wayland surface support compiles but is runtime-validated only on a
Wayland session (see ADR 0005).

## Tests

```bash
ctest --preset debug -R '^render\.'          # camera + RHI value types (CI-safe)
zukiru_render_tests "[.gpu]"                 # real GPU: buffers/textures/pipelines
```

Camera math and RHI value types are covered by ordinary unit tests. The hidden
`[.gpu]` tests open a real window and, on the actual device: draw user geometry
from a vertex buffer; reject invalid SPIR-V; and draw a **textured, uniform-
transformed** triangle (uniform `mat4` buffer + a 2×2 checkerboard texture + a
bind group) — 20 frames with a resize. Verified on an NVIDIA RTX 3060, clean under
ASan. The demo shaders live in [`tests/shaders/`](tests/shaders) (GLSL) and are
embedded as SPIR-V by `zukiru-shaderc` — so the render module itself needs no
shader compiler at build or run time.

## Dependencies

`core`, `math`, `platform` (Layer 2); Vulkan (private). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).

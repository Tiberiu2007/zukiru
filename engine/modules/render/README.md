# render

**Layer 2 — core subsystems.** An RHI (render hardware interface) abstraction with
a **Vulkan** backend, plus cameras. Game/scene code talks to the backend-agnostic
RHI, never to Vulkan directly, so a second backend (D3D12/Metal) can drop in later.

It brings up the full Vulkan chain (instance → surface → device → swapchain →
depth buffer → render pass → present) and exposes **GPU buffers** (vertex/index +
per-frame ring-buffered uniforms), **push constants**, **textures**, **SPIR-V
graphics pipelines**, **bind groups** (uniform + texture descriptor sets), **depth
testing**, and **per-frame command recording** — enough to draw your own textured,
depth-tested 3D geometry. On top of the RHI it adds **materials** (a pipeline + named
parameters) and a **render graph** (a frame organizer), and it ships **cameras**,
**primitive mesh builders**, and **offscreen render targets** (render into a texture,
sample it in a later pass). See [ADR 0006](../../../docs/adr/0006-render-architecture.md),
[ADR 0008](../../../docs/adr/0008-render-graph-and-materials.md),
[ADR 0009](../../../docs/adr/0009-offscreen-render-targets.md), and
[ADR 0010](../../../docs/adr/0010-per-frame-uniforms-and-push-constants.md).
Namespace `zuki::render`.

## Drawing your own geometry

```cpp
#include <zuki/render/render.hpp>

auto device = render::createDevice(*window).value();

// Upload a vertex buffer and build a pipeline that reads it.
auto vbo = device->createBuffer(render::BufferKind::Vertex, verts, sizeof(verts));
render::PipelineDesc desc;
desc.vertexSpirv   = vertSpirv;    // std::span<const u32> — cooked by zuki-shaderc
desc.fragmentSpirv = fragSpirv;
desc.vertexLayout.stride = sizeof(Vertex);
desc.vertexLayout.attributes = {
    {.location = 0, .format = render::VertexFormat::Float32x2, .offset = 0},
    {.location = 1, .format = render::VertexFormat::Float32x3, .offset = 8},
};
auto pipeline = device->createPipeline(desc).value();

while (running) {
    if (!device->beginFrame()) { device->resize(w, h); continue; }
    device->beginSwapchainPass();     // open a pass before recording draws
    device->bindPipeline(pipeline);
    device->bindVertexBuffer(vbo);
    device->draw(3);
    device->endRenderPass();
    device->endFrame();
}
```

`Device` is **backend-agnostic** — no Vulkan types leak into the public headers.
It exposes: resources (`createBuffer` (Vertex/Index/Uniform) + `updateBuffer`,
`createTexture` (RGBA8), `createRenderTarget`, `createPipeline`, `createBindGroup`,
and `destroy*` — opaque `BufferHandle`/`TextureHandle`/`RenderTargetHandle`/
`PipelineHandle`/`BindGroupHandle`); the **frame** (`beginFrame` acquires, `endFrame`
submits + presents) and **passes** within it (`beginSwapchainPass` /
`beginRenderPass(target)` / `endRenderPass` — one open at a time); recording inside a
pass (`bindPipeline`, `bindBindGroup`, `bindVertexBuffer`, `bindIndexBuffer`,
`pushConstants`, `draw`, `drawIndexed`); plus `setClearColor`, `resize`, `waitIdle`,
`backend()`, `deviceName()`.

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

Uniform buffers are **ring-buffered per frame-in-flight**: `updateBuffer` writes
only the copy the current frame reads (selected by a dynamic offset at bind time),
so re-uploading every frame is safe even while earlier frames are still in flight —
no stalls, no torn reads. Because it updates just that copy, update every frame you
draw (values set at creation persist across all copies).

## Push constants

For small **per-draw** data — a model matrix, a tint — push constants beat a uniform
buffer per object: they're recorded straight into the command buffer, so there's no
buffer, no descriptor, and no synchronization to worry about. Declare a size on the
pipeline and push before each draw:

```cpp
desc.pushConstantSize = sizeof(math::Mat4);   // a layout(push_constant) block, both stages
auto pipeline = device->createPipeline(desc).value();

// shared per-frame data in a uniform, per-object data in the push constant:
device->bindPipeline(pipeline); device->bindBindGroup(cameraGroup);
for (const Object& o : objects) {
    device->pushConstants(o.model.e, sizeof(o.model.e));
    device->drawIndexed(o.indexCount);
}
```

Keep it small — the spec guarantees only 128 bytes (a `mat4` is 64). Larger or
per-frame-shared data belongs in a ring-buffered uniform.

Pipelines take **SPIR-V** (`std::span<const u32>`) — produce it offline with
[`zuki-shaderc`](../../../tools/shader_compiler) and embed or load it. Viewport/
scissor are dynamic, so pipelines survive window resizes without a rebuild.

## Depth testing and 3D meshes

Every frame clears and renders through a **depth attachment** sized to the
swapchain, so 3D geometry occludes correctly without any caller setup. A pipeline
opts into depth via `PipelineDesc` (both on by default — the right choice for
opaque geometry):

```cpp
desc.depthTest = true;   // discard fragments behind what's already drawn
desc.depthWrite = true;  // record this fragment's depth (turn off for overlays)
```

`primitives.hpp` builds CPU geometry ready to upload — a `MeshData` is
interleaved `MeshVertex` (position, normal, uv; stride 32) plus a `u16` index
buffer:

```cpp
const render::MeshData cube = render::cubeMesh();
auto vbo = device->createBuffer(BufferKind::Vertex, cube.vertices.data(), cube.vertexBytes());
auto ibo = device->createBuffer(BufferKind::Index, cube.indices.data(), cube.indexBytes());
// attributes: pos @0 (Float32x3), normal @12 (Float32x3), uv @24 (Float32x2)
// per frame: bindVertexBuffer(vbo); bindIndexBuffer(ibo, IndexType::U16); drawIndexed(36);
```

## Materials

A **material** bundles a pipeline with its shader parameters so callers stop
hand-packing uniform buffers and building descriptor sets. It's a small stack on
the RHI (ADR 0008), split so the packing logic is GPU-free and testable:

- `MaterialLayout` — the schema: named **std140** uniform members
  (`ParamType::Float/Vec2/Vec3/Vec4/Mat4`) + named texture slots. Computes each
  member's byte offset, the block size, and the RHI `bindings()` a matching shader
  declares (binding 0 = the uniform block, then one texture per slot).
- `MaterialParams` — a CPU byte block written through named setters.
- `MaterialTemplate` — owns the pipeline for a layout (shared across instances).
- `Material` — a template instance owning a uniform buffer + textures + bind group.

```cpp
MaterialLayout layout;
layout.addMat4("mvp").addMat4("model").addTexture("tex");   // matches cube.{vert,frag}

auto tmpl = MaterialTemplate::create(device, {.layout = layout,
    .vertexSpirv = vs, .fragmentSpirv = fs, .vertexLayout = vl}).value();
auto material = tmpl->instantiate();
material->setTexture("tex", albedo);

// per frame, inside a render pass:
material->setMat4("mvp", camera.viewProjection() * model).setMat4("model", model);
material->bind(device);   // uploads dirty uniforms, (re)builds the bind group, binds
device.bindVertexBuffer(vbo); device.drawIndexed(count);
```

## Render graph

A **render graph** organizes a frame into passes with explicit resource
dependencies. `compile()` orders passes so producers run before consumers, rejects
dependency cycles, and **culls passes whose results nothing consumes**; `execute()`
runs the live passes in order.

```cpp
RenderGraph graph;
const RgResource shadow      = graph.createResource("shadowMap");
const RgResource backbuffer  = graph.importResource("backbuffer");  // external
graph.addPass("shadow").writes(shadow).setExecute([&](PassContext& c) { /* record */ });
graph.addPass("opaque").reads(shadow).writes(backbuffer).setExecute([&](PassContext& c) {
    material->bind(c.device); c.device.drawIndexed(count);
});

if (auto compiled = graph.compile(); compiled) {
    if (device.beginFrame()) { graph.execute(device, compiled.value()); device.endFrame(); }
}
```

Passes writing an **imported** resource are always live (observable side effects);
everything else survives only if it feeds one. The graph doesn't yet allocate
physical render targets from its virtual resources automatically — that transient
allocation + auto-barrier layer is **deferred** (ADR 0008/0009) and slots in without
changing this API. The RHI primitive it needs — offscreen render targets — now
exists and can be wired by hand:

## Render targets

A **render target** is a texture you draw into (in one pass) and sample (in a
later one) — the basis of shadow maps, post-processing, and deferred shading. A
frame separates the **frame** (`beginFrame`/`endFrame`) from **passes**; open a pass
into a target or the swapchain, one at a time:

```cpp
auto target = device->createRenderTarget({.width = 512, .height = 512,
                                          .clearColor = {0.1f, 0.05f, 0.2f, 1.0f}});
auto targetTex = device->renderTargetTexture(target);   // its color, as a texture
// ...a fullscreen pipeline + bind group that sample targetTex...

if (device->beginFrame()) {
    device->beginRenderPass(target);      // pass 1 → offscreen
    device->bindPipeline(scenePipeline); device->drawIndexed(count);
    device->endRenderPass();

    device->beginSwapchainPass();         // pass 2 → screen, sampling the target
    device->bindPipeline(fullscreenPipeline); device->bindBindGroup(postGroup);
    device->draw(3);
    device->endRenderPass();
    device->endFrame();
}
```

A target's color attachment uses the **swapchain's format** and gets a depth buffer,
so any pipeline built for the window renders into it unchanged. Its render pass
leaves the color in a shader-readable layout with the write→sample dependency baked
in — the next pass's sampling waits automatically, no manual barrier. Targets are
fixed-size (they don't follow swapchain resizes) and own their color texture (don't
`destroyTexture` it — `destroyRenderTarget` frees it).

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
logical device, swapchain (format/present-mode/extent), a **depth attachment**
(`D32_SFLOAT` preferred, recreated with the swapchain), a two-attachment render
pass (color + depth, both `loadOp=CLEAR`), framebuffers, command buffers, and
per-frame sync (2 frames in flight). Buffers are host-visible/coherent (map + memcpy);
uniform buffers are allocated as one slice per frame-in-flight (aligned to
`minUniformBufferOffsetAlignment`) and bound as `UNIFORM_BUFFER_DYNAMIC` with a
per-frame dynamic offset, so `updateBuffer` never races an in-flight read. Pipelines
can reserve a push-constant range (both stages); textures
upload through a staging buffer with image-layout transitions (single-time command
submits) and get a view + linear sampler. Pipelines are built from the
`PipelineDesc` (SPIR-V + vertex layout + topology + depth test/write + a
descriptor-set-0 layout from `bindings`) with dynamic viewport/scissor and a
`LESS_OR_EQUAL` depth compare; bind groups are descriptor sets from a shared pool.
Out-of-date/suboptimal swapchains and `resize()` trigger recreation (depth
attachment included); pipelines and resources are unaffected. Offscreen render
targets are a sampleable color image (swapchain format, registered in the texture
table) + a depth image, wrapped in their own render pass (color `finalLayout =
SHADER_READ_ONLY` with write→sample subpass dependencies) and framebuffer.

- **Vulkan is a private dependency**: headers come from a system SDK or, failing
  that, **Vulkan-Headers via FetchContent** (a clean box ships only
  `libvulkan.so.1`). Public consumers see only the RHI.
- The X11 surface uses its **own** `Display` connection, closed while the instance
  is still alive — this sidesteps an NVIDIA driver crash where `XCloseDisplay`
  invokes a hook into an already-unloaded ICD.

## Scope

Buffers (vertex/index + ring-buffered uniforms), push constants, RGBA8 textures,
offscreen render targets, SPIR-V pipelines with uniform + texture bind groups, depth
testing, explicit passes + command recording (draw / draw-indexed), materials
(pipeline + std140 params + textures), a render graph (pass scheduling + culling),
cameras, primitive meshes (`cubeMesh`).
**Deferred** (additive, no API break): staging/DEVICE_LOCAL vertex buffers + a real
GPU sub-allocator (vertex/uniform buffers are host-visible today), mipmaps, MSAA,
depth-only/HDR render targets + MRT, the render graph's **physical transient-target
allocation + automatic barriers** (the render-target primitive now exists; the graph
doesn't auto-allocate yet), and additional backends (D3D12/Metal). Wayland surface
support compiles but is runtime-validated only on a Wayland session (see ADR 0005).

## Tests

```bash
ctest --preset debug -R '^render\.'          # camera/RHI/primitives/material/graph (CI-safe)
zuki_render_tests "[.gpu]"                 # real GPU: pipelines/depth/materials/graph
```

Camera math, RHI value types, `cubeMesh` geometry, **material std140 packing**, and
**render-graph scheduling** (dependency ordering, cycle rejection, dead-pass culling,
execute dispatch via a stub device) are covered by ordinary unit tests. The hidden
`[.gpu]` tests open a real window and, on the actual device: draw user geometry from
a vertex buffer; reject invalid SPIR-V; draw a **textured, uniform-transformed**
triangle; draw a **depth-tested rotating textured cube** (indexed `cubeMesh`,
perspective camera, near faces occluding far); draw a **material through a render
graph** (the cube, its uniforms/bind group owned by a `Material` recorded inside a
graph pass); **render offscreen then composite to screen** (the cube into a 512²
render target, sampled onto the window by a vertexless fullscreen triangle — two
passes in one frame); and draw a **3×3 grid of independently-spinning cubes** sharing
one per-frame camera uniform (re-uploaded every frame, ring-buffered) with a per-cube
model matrix pushed as a **push constant**. Verified on an NVIDIA RTX 3060, clean
under ASan. The demo
shaders live in [`tests/shaders/`](tests/shaders) (GLSL) and are embedded as SPIR-V
by `zuki-shaderc` — so the render module itself needs no shader compiler at build
or run time.

## Dependencies

`core`, `math`, `platform` (Layer 2); Vulkan (private). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).

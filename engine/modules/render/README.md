# render

**Layer 2 — core subsystems.** An RHI (render hardware interface) abstraction with
a **Vulkan** backend, plus cameras. Game/scene code talks to the backend-agnostic
RHI, never to Vulkan directly, so a second backend (D3D12/Metal) can drop in later.

It brings up the full Vulkan chain (instance → surface → device → swapchain →
depth buffer → render pass → present) and exposes **GPU buffers**
(vertex/index/uniform), **textures**, **SPIR-V graphics pipelines**, **bind
groups** (uniform + texture descriptor sets), **depth testing**, and **per-frame
command recording** — enough to draw your own textured, depth-tested 3D geometry.
On top of the RHI it adds **materials** (a pipeline + named parameters) and a
**render graph** (a frame organizer), and it ships **cameras** and **primitive
mesh builders**. See [ADR 0006](../../../docs/adr/0006-render-architecture.md) and
[ADR 0008](../../../docs/adr/0008-render-graph-and-materials.md). Namespace `zukiru::render`.

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
everything else survives only if it feeds one. Today every pass records into the
current framebuffer — physical allocation of transient render targets and automatic
barriers are **deferred** until the RHI gains offscreen targets (ADR 0008), and slot
in without changing this API.

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
per-frame sync (2 frames in flight). Buffers are host-visible/coherent (map + memcpy); textures
upload through a staging buffer with image-layout transitions (single-time command
submits) and get a view + linear sampler. Pipelines are built from the
`PipelineDesc` (SPIR-V + vertex layout + topology + depth test/write + a
descriptor-set-0 layout from `bindings`) with dynamic viewport/scissor and a
`LESS_OR_EQUAL` depth compare; bind groups are descriptor sets from a shared pool.
Out-of-date/suboptimal swapchains and `resize()` trigger recreation (depth
attachment included); pipelines and resources are unaffected.

- **Vulkan is a private dependency**: headers come from a system SDK or, failing
  that, **Vulkan-Headers via FetchContent** (a clean box ships only
  `libvulkan.so.1`). Public consumers see only the RHI.
- The X11 surface uses its **own** `Display` connection, closed while the instance
  is still alive — this sidesteps an NVIDIA driver crash where `XCloseDisplay`
  invokes a hook into an already-unloaded ICD.

## Scope

Buffers (vertex/index/uniform), RGBA8 textures, SPIR-V pipelines with uniform +
texture bind groups, depth testing, command recording (draw / draw-indexed),
materials (pipeline + std140 params + textures), a render graph (pass scheduling +
culling), cameras, primitive meshes (`cubeMesh`).
**Deferred** (additive, no API break): staging/DEVICE_LOCAL vertex buffers + a real
GPU allocator (vertex/uniform buffers are host-visible today), per-frame uniform
ring buffers (`updateBuffer` must not race an in-flight frame), push constants,
mipmaps, MSAA, **offscreen render targets** (and with them the render graph's
physical transient-resource allocation + automatic barriers), and additional
backends (D3D12/Metal). Wayland surface support compiles but is runtime-validated
only on a Wayland session (see ADR 0005).

## Tests

```bash
ctest --preset debug -R '^render\.'          # camera/RHI/primitives/material/graph (CI-safe)
zukiru_render_tests "[.gpu]"                 # real GPU: pipelines/depth/materials/graph
```

Camera math, RHI value types, `cubeMesh` geometry, **material std140 packing**, and
**render-graph scheduling** (dependency ordering, cycle rejection, dead-pass culling,
execute dispatch via a stub device) are covered by ordinary unit tests. The hidden
`[.gpu]` tests open a real window and, on the actual device: draw user geometry from
a vertex buffer; reject invalid SPIR-V; draw a **textured, uniform-transformed**
triangle; draw a **depth-tested rotating textured cube** (indexed `cubeMesh`,
perspective camera, near faces occluding far); and draw a **material through a render
graph** (the cube again, its uniforms/bind group owned by a `Material` recorded
inside a graph pass). Verified on an NVIDIA RTX 3060, clean under ASan. The demo
shaders live in [`tests/shaders/`](tests/shaders) (GLSL) and are embedded as SPIR-V
by `zukiru-shaderc` — so the render module itself needs no shader compiler at build
or run time.

## Dependencies

`core`, `math`, `platform` (Layer 2); Vulkan (private). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).

# renderer

**Layer 3 — gameplay.** A high-level **scene renderer** over the `render` RHI: it
lets an ECS entity *draw itself*. Attach a `MeshRenderer` to any entity that also
has a `scene::WorldTransform`, and `renderMeshes()` draws them all — so games (and
the editor) stop hand-wiring the per-entity `bind*` / `pushConstants` / `draw` loop.
It bridges **render + scene + ecs**; the RHI and scene graph stay independent of
each other. Namespace `zuki::renderer`.

## Usage

```cpp
#include <zuki/renderer/renderer.hpp>

// Once: upload geometry, attach a MeshRenderer to each drawable node.
renderer::Mesh cube = renderer::uploadMesh(device, render::cubeMesh());
world.add<renderer::MeshRenderer>(entity,
    {.mesh = cube, .pipeline = pipeline, .bindGroup = cameraAndTexture});

// Per frame, inside a render pass (the caller owns the frame + per-frame uniforms):
device.updateBuffer(cameraUbo, viewProj.e, sizeof(viewProj.e));  // e.g. the camera
renderer::renderMeshes(device, scene.world());
```

`renderMeshes` iterates every entity with a `scene::WorldTransform` + `MeshRenderer`
and records the draws into the **currently open render pass**. It **minimizes state
changes** — pipeline / bind-group / vertex / index binds are re-issued only when
they differ from the previous draw — and pushes each entity's world matrix
(`WorldTransform::toMatrix()`) as a **push constant**.

### Contract

- The `pipeline` must declare `layout(push_constant) uniform { mat4 model; }` and be
  created with `pushConstantSize = sizeof(math::Mat4)` — that's how the per-object
  transform arrives.
- `bindGroup` supplies the pipeline's shared/per-material resources (e.g. a per-frame
  camera uniform + a texture); leave it invalid for a pipeline that reads none.
- The caller owns the frame (`beginFrame`/pass/`endFrame`) and any per-frame uniform
  updates. `renderMeshes` only records draw commands.

## Meshes

`uploadMesh(device, render::MeshData)` moves interleaved CPU geometry (e.g. from
`render::cubeMesh()`) into GPU vertex/index buffers and returns a `Mesh`
(handles + element count); `destroyMesh(device, mesh)` frees them. A `Mesh` with no
index buffer draws non-indexed. The vertex *layout* is the pipeline's concern —
`uploadMesh` only moves bytes.

## Design

The renderer sits **above** the Layer-2 subsystems it unites, so neither `render`
(the RHI) nor `scene` gains a dependency on the other or on the ECS. Draw-call
sorting today is a cheap "don't rebind what's already bound" pass in ECS iteration
order; a full material/depth sort is a natural, additive extension. Instancing,
frustum culling, and multiple sub-meshes per entity are deferred.

## Tests

CI-safe and GPU-free: a **recording `Device`** captures the exact `bind*` /
`pushConstants` / `draw` sequence, so the tests assert one draw per entity, correct
state-change minimization (shared pipeline/mesh bound once across consecutive
entities), skipping of empty meshes, the non-indexed `draw()` path, and that
transform-only nodes (no `MeshRenderer`) aren't drawn. The real-GPU path is covered
by [`games/sandbox`](../../../games/sandbox), which drives `renderMeshes` on an
RTX 3060 (clean under ASan).

## Dependencies

`core`, `math`, `render`, `scene`, `ecs`. See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).

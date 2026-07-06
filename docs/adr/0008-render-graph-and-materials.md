# ADR 0008 — Render graph + materials

- **Status:** Accepted
- **Date:** 2026-07-06
- **Deciders:** claude-opus-4-8 (at the project owner's direction)
- **Related:** `engine/modules/render/`, ADR 0006 (render architecture),
  Milestone 3 in `agents/TODO.md`.

## Context

The RHI (ADR 0006, since extended with buffers, textures, pipelines, bind groups
and depth) lets a caller draw geometry, but two jobs are still done by hand:

1. **Binding shader parameters.** A caller manually creates a uniform buffer,
   packs floats/matrices into it with the right std140 offsets, creates a texture,
   builds a bind group, and re-uploads on every change. Every draw site
   re-implements this. Gameplay/scene code wants to say "this object uses *this*
   material with *these* values."
2. **Organizing a frame.** A frame is a sequence of passes (shadow, opaque,
   transparent, post, UI) with data dependencies between them (pass B samples what
   pass A rendered). Ordering and resource lifetimes are currently implicit in the
   order the caller writes code.

Both are classic engine layers — **materials** and a **render graph** (frame
graph). Both must stay **backend-agnostic**: they sit *on top of* the RHI and leak
no Vulkan types, consistent with ADR 0006.

The honest constraint: the RHI today renders only into the **swapchain**. There is
no offscreen render-target abstraction, so a render graph cannot yet allocate the
transient textures that passes would read from one another. Building the physical
transient-resource allocator + automatic barriers now would front-run a
render-target RHI extension and couple the graph to Vulkan specifics.

## Decision

Ship both layers, each scoped to a **fully-tested logical core** on the RHI, with
the backend-heavy parts deferred additively (the ADR 0006 pattern).

### Materials (`render/material.hpp` + `material.cpp`)

A three-part split so the packing logic is testable without a GPU:

- **`MaterialLayout`** (pure) — the schema: an ordered list of named uniform
  members (`ParamType` Float/Vec2/Vec3/Vec4/Mat4) laid out with **std140** rules
  (vec3 aligns to 16, mat4 is 64 bytes, block padded to 16), plus named **texture
  slots**. Computes each member's byte offset, the total block size, and the RHI
  `bindings()` list: binding 0 = the uniform block (when there are params), then
  one `Texture` binding per slot. No RHI/Vulkan objects.
- **`MaterialParams`** (pure) — a value pairing a layout with a CPU byte block;
  `setFloat/Vec*/Mat4(name, …)` write into the block at the layout's offsets.
  Fully CI-testable (round-trip the bytes).
- **`MaterialTemplate`** (GPU) — owns the `PipelineHandle` built from shaders +
  vertex layout + the layout's `bindings()`. Shared across instances.
- **`Material`** (GPU) — a template instance: owns a uniform buffer, the per-slot
  texture handles, and a bind group. `set*` mutate its `MaterialParams`; `bind()`
  uploads dirty uniforms (`updateBuffer`), lazily (re)builds the bind group when
  textures change, and records `bindPipeline` + `bindBindGroup`.

A material's `bindings()` is exactly what a matching GLSL shader declares, so a
material with `mat4 mvp; mat4 model;` params + a `tex` slot drives the existing
`cube.{vert,frag}` shaders unchanged.

### Render graph (`render/render_graph.hpp` + `render_graph.cpp`)

A frame organizer that **compiles** a declared pass/resource DAG and **executes**
live passes in dependency order:

- Virtual resources (`RgResource`), created or *imported* (external, e.g. the
  backbuffer). Passes declare `reads(r)` / `writes(r)` and an `execute` callback
  taking a `PassContext` (holds the `Device&`).
- `compile()` builds read-after-write edges (a reader depends on every writer of a
  resource it reads), **topologically sorts** them (Kahn), **detects cycles**
  (error), and **culls dead passes** — those not reachable (over the reverse graph)
  from a pass that writes an imported/output resource. Returns the ordered live
  passes or an `Error`.
- `execute(Device&)` runs the compiled order, invoking each pass's callback. The
  caller wraps it in `beginFrame` / `endFrame`.

This is the scheduling/organization core — pure and CI-tested (ordering respects
dependencies, cycles rejected, dead passes dropped).

### Deferred (additive, no public break)

- **Offscreen render targets** in the RHI, and with them the graph's **physical
  transient-resource allocation + aliasing** and **automatic pipeline barriers**.
  Until then all passes render to the current framebuffer.
- Material features: parameter animation, instancing, render-state (blend/cull)
  beyond depth, shader variants/permutations, a material asset format.

## Consequences

- Gameplay/scene code gets a real "material" abstraction and a place to express
  multi-pass frames, both backend-agnostic.
- The valuable, bug-prone logic (std140 packing, DAG scheduling) is covered by
  fast CI-safe tests; a `[.gpu]` test proves the material + graph execute path on
  the real device (a cube drawn through a `Material` inside a graph pass).
- The deferred pieces are exactly the ones that need a render-target RHI; they slot
  in without changing the material/graph API.

## Alternatives considered

- **Full frame graph now** (transient allocation + barrier generation) — rejected:
  needs offscreen render targets the RHI lacks, and would bake in Vulkan barrier
  logic before the target abstraction exists.
- **Immediate-mode material = pipeline + a raw bind group** (no layout/params) —
  rejected: pushes std140 packing back onto every caller, the exact duplication
  this layer removes.
- **Materials in a separate module** — rejected: they are pure RHI clients with no
  new dependencies; a sibling header in `render` keeps the layering flat.

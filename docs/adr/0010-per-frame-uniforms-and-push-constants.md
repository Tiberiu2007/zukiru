# ADR 0010 — Per-frame uniform ring buffers + push constants

- **Status:** Accepted
- **Date:** 2026-07-06
- **Deciders:** claude-opus-4-8 (at the project owner's direction)
- **Related:** `engine/modules/render/`, ADR 0006 (render architecture), ADR 0008
  (materials), Milestone 3/4 in `agents/TODO.md`.

## Context

Uniform data is updated **every frame** (a camera matrix, per-object transforms),
but the RHI ran two **frames in flight**, and uniform buffers were a single
host-visible allocation updated in place by `updateBuffer`. Writing frame N's data
while frame N-1 is still being read by the GPU is a **write-after-read hazard** —
the exact "must not race an in-flight frame" caveat the earlier docs flagged. It was
invisible only because a one-frame-stale matrix looks fine and neither ASan nor
(unavailable) validation layers catch a GPU-side data race.

Separately, per-**draw** data (a model matrix per object) had no cheap path: every
object needed its own uniform buffer + bind group.

## Decision

Two complementary additions, both keeping the RHI backend-agnostic.

### Per-frame uniform ring buffers (dynamic offsets)

Every uniform buffer (`BufferKind::Uniform`) is allocated as **N slices**, one per
frame-in-flight, each aligned to the device's `minUniformBufferOffsetAlignment`.
Its descriptor is a `UNIFORM_BUFFER_DYNAMIC`, and `bindBindGroup` supplies a
**dynamic offset of `currentFrame * sliceSize`** — so the frame reads its own slice.
`updateBuffer` writes only the current frame's slice; `createBuffer` seeds every
slice so all frames start valid. Result: updating a uniform every frame is **safe by
construction** — no cross-frame race, no stalls.

Consequence for the semantics: a uniform written once mid-run only updates one
slice, so the contract is "update every frame you draw" (values set at creation
persist across all slices). `Material::bind` therefore uploads its params **every
bind** rather than tracking a dirty flag — cheap, and it keeps every slice current.

### Push constants

`PipelineDesc::pushConstantSize` reserves a small `push_constant` block (both
stages) in the pipeline layout; `Device::pushConstants(data, size)` records it
straight into the command buffer against the currently-bound pipeline. Per-draw data
with **no buffer, no descriptor, and no race** — ideal for a model matrix (64 bytes;
the spec guarantees at least 128).

The two features split the problem cleanly: **push constants** for per-draw data,
**ring-buffered uniforms** for per-frame data shared across many draws.

## Consequences

- Animating uniforms is correct under multiple frames in flight; the earlier
  "don't update in flight" footgun is gone.
- Drawing many moving objects is cheap and idiomatic: one shared camera uniform +
  a push-constant model matrix per draw. A `[.gpu]` test drives a 3×3 grid of
  independently-spinning cubes this way, camera orbiting (uniform re-uploaded every
  frame) — verified on an RTX 3060, clean under ASan.
- Uniform memory grows by the frames-in-flight factor (negligible for uniforms) and
  is rounded up to the alignment; a real sub-allocator is still deferred.
- No public Vulkan types; the dynamic-offset machinery is entirely inside the
  backend.

## Alternatives considered

- **N separate descriptor sets per bind group (one per frame)** — avoids dynamic
  offsets but multiplies descriptor sets and rebinding. Dynamic offsets keep one set
  and one buffer per uniform. Rejected.
- **Serialize on a fence before each `updateBuffer`** — correct but throws away the
  point of frames-in-flight. Rejected.
- **Only push constants (no uniform fix)** — push constants are capped (~128 bytes)
  and per-draw; they don't cover larger per-frame data shared across draws. Both are
  needed.

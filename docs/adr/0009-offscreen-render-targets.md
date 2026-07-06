# ADR 0009 — Offscreen render targets + explicit render passes

- **Status:** Accepted
- **Date:** 2026-07-06
- **Deciders:** claude-opus-4-8 (at the project owner's direction)
- **Related:** `engine/modules/render/`, ADR 0006 (render architecture), ADR 0008
  (render graph + materials), Milestone 3 in `agents/TODO.md`.

## Context

The RHI could render only into the **swapchain** — `beginFrame()` implicitly opened
the one on-screen render pass, and every draw landed there. That blocks the whole
family of "render into a texture, then read that texture" techniques (shadow maps,
post-processing, reflections, deferred shading) and leaves the render graph (ADR
0008) scheduling passes against resources it can't physically express.

Adding offscreen targets forces a second decision: a single frame must now contain
**more than one render pass** (draw into the target, then into the swapchain), and
Vulkan render passes cannot nest. So the frame lifecycle has to separate "the frame"
(acquire/submit/present) from "a pass" (a render-pass scope you record draws into).

## Decision

### Explicit passes (frame ≠ render pass)

`beginFrame()` now only acquires the swapchain image and opens the command buffer;
`endFrame()` only submits + presents. Recording happens inside an explicitly-opened
pass:

- `beginSwapchainPass()` — render into the window (cleared to `setClearColor`).
- `beginRenderPass(RenderTargetHandle)` — render into an offscreen target (cleared
  to its own clear color).
- `endRenderPass()` — close the open pass.

Exactly one pass is open at a time; a frame may open several in sequence. This is a
breaking change to the ~3 call sites (the `app` loop and the GPU tests), all updated.

### Render targets (`RenderTargetDesc` / `RenderTargetHandle`)

`createRenderTarget({width, height, clearColor})` builds a **color attachment** (the
swapchain's format, `COLOR_ATTACHMENT | SAMPLED`) plus a **depth attachment**
(`D32`), a matching render pass, and a framebuffer. `renderTargetTexture()` exposes
the color attachment as a normal `TextureHandle` (it is registered in the device's
texture table), so it drops straight into a bind group for the sampling pass.

Two deliberate constraints keep it simple and correct:

- **Color format = swapchain format, depth always present.** Vulkan requires a
  pipeline's render pass to be *format-compatible* with the framebuffer's. Matching
  the swapchain's attachments means **any pipeline built for the window renders into
  an offscreen target unchanged** — no per-target pipeline variants.
- **Render-pass-driven synchronization.** The target's render pass sets the color
  attachment's `finalLayout = SHADER_READ_ONLY_OPTIMAL` and carries two subpass
  dependencies (incoming: wait for prior sampling before overwriting; outgoing: make
  the next pass's fragment sampling wait for color writes). No manual
  `vkCmdPipelineBarrier` — the barrier is expressed where the hazard is.

### Scope / deferred

- **Graph-driven transient allocation.** Render targets exist and can be wired by
  hand, but the render graph does **not** yet allocate/alias them from its virtual
  `RgResource`s automatically (ADR 0008's deferral stands — that layer now has the
  RHI primitive it was waiting on).
- Depth-only targets (shadow maps sampling depth), non-swapchain color formats
  (HDR/linear), MRT, and MSAA remain deferred — all additive.

## Consequences

- The canonical multi-pass frame is now expressible: a `[.gpu]` test renders the
  cube into a 512² target and composites it to the screen with a vertexless
  fullscreen triangle sampling that target — verified on an RTX 3060, clean under
  ASan.
- `Device` grew `createRenderTarget` / `destroyRenderTarget` / `renderTargetTexture`
  and the pass trio; `beginFrame`/`endFrame` no longer imply a pass. Still no Vulkan
  types in public headers.
- The next step for the render graph (physical transient resources + auto-barriers)
  is now unblocked and purely additive.

## Alternatives considered

- **Keep `beginFrame` opening the swapchain pass; bolt offscreen rendering on** —
  impossible: passes can't nest, so the frame must not own a pass.
- **Per-target color formats with on-demand pipeline variants** — flexible but drags
  in a pipeline-compatibility/caching layer for no current benefit. Deferred by
  fixing offscreen color to the swapchain format.
- **Manual barriers instead of render-pass layout transitions** — more code and
  easier to get wrong than letting the render pass express the layout change and the
  dependency. Rejected.

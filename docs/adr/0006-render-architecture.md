# ADR 0006 — Render architecture: RHI abstraction + Vulkan first backend

- **Status:** Accepted
- **Date:** 2026-07-05
- **Deciders:** claude-opus-4-8 (at the project owner's direction)
- **Related:** `engine/modules/render/`, ADR 0005 (windowing), Milestone 3/4 in
  `agents/TODO.md`.

## Context

The engine needs a rendering module. Two forces shape its first cut:

1. **Portability** — we want to target Vulkan first but not weld the engine to it;
   D3D12/Metal backends are on the roadmap. So gameplay/scene code must talk to a
   backend-agnostic **RHI** (render hardware interface), not to Vulkan directly.
2. **What's actually buildable now** — the dev box has the Vulkan **loader** and a
   real device (NVIDIA RTX 3060, plus a llvmpipe software fallback), but **no dev
   headers and no SPIR-V shader compiler** (`glslc`/`glslangValidator` absent), and
   the `shader_compiler` tool is a later milestone (M4).

Drawing a triangle needs shaders → a SPIR-V toolchain. Clearing the screen does
not. Bringing up instance → device → surface → swapchain → render pass → present
exercises ~90% of the hard Vulkan plumbing **without any shaders**.

## Decision

Ship a **clear-screen MVP** behind a small RHI, with Vulkan as the only backend.

### RHI (`render/rhi.hpp`, backend-agnostic, no Vulkan types leak)

- `Backend` enum, `Color`, `DeviceConfig` (validation, vsync).
- Abstract `Device`: `beginFrame()` (acquire; returns false if the swapchain is
  out of date) / `endFrame()` (submit + present) / `setClearColor()` / `resize()`
  / `waitIdle()` / `backend()` / `deviceName()`.
- `createDevice(const platform::Window&, DeviceConfig)` → `Result<unique_ptr<Device>>`.

Command recording, resources (buffers/textures), pipelines and shaders are **not**
in this first cut — they extend `Device`/add types without changing what exists.

### Vulkan backend (`render/src/vulkan/`, private)

Instance (+ optional validation layer & debug messenger) → surface (from the
window's native handle; Xlib or Wayland per `Window::nativeBackend()`) → physical
device selection (graphics+present, swapchain ext; prefers discrete) → logical
device + queues → swapchain (format/present-mode/extent) → image views → a
single-attachment render pass (loadOp=CLEAR) → framebuffers → command pool/buffers
→ per-frame sync (image-available/render-finished semaphores, in-flight fences).
`beginFrame` acquires + records the clear; `endFrame` submits + presents; out-of-
date/suboptimal or `resize()` triggers swapchain recreation.

### Dependencies & build

- **Vulkan headers** via `zuki_require_vulkan()`: a system SDK if present, else
  **Vulkan-Headers** through FetchContent (the box ships only `libvulkan.so.1`).
  The loader is found even when only the versioned soname exists.
- Vulkan is a **private** dependency of `render` — public consumers see only the
  RHI. The backend includes the Vulkan platform-surface headers (Xlib/Wayland)
  for the surface `create-info` types but does **not** link libX11/libwayland
  (surface creation goes through the Vulkan loader). Those surface paths compile
  conditionally on the same detection the platform module uses.

### Cameras (`render/camera.hpp`, pure math)

A `Camera` producing view and projection (perspective/orthographic) matrices via
`math`. Independent of the backend and fully unit-testable.

## Consequences

- **Verifiable now**: a hidden `[.gpu]` test opens a real window and clears/presents
  frames on the actual GPU. RHI/camera logic is covered by ordinary CI-safe tests.
- **No shader dependency** pulled in early; the triangle arrives with the M4 shader
  toolchain, at which point pipelines/materials extend the RHI.
- Backend-agnostic surface means `app` and gameplay never include Vulkan.
- **Deferred** (additive): command/resource/pipeline RHI, shaders + first triangle,
  render graph, materials, MSAA/depth, multiple backends (D3D12/Metal).

## Alternatives considered

- **Vendor a shader compiler now (glslang) to draw a triangle** — rejected for the
  first cut: glslang is a heavy from-source build and front-runs the M4
  `shader_compiler` tool. Revisit when that tool lands.
- **Expose Vulkan handles in the public API** (no RHI) — faster short-term, but
  welds the engine to Vulkan and contradicts the multi-backend roadmap. Rejected.
- **Headless/offscreen only** — simpler (no surface), but doesn't prove the
  swapchain/present path the engine actually needs. Rejected.

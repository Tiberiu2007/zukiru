# sandbox

The first **playable Zuki demo** — and the first consumer of the engine *as a
game*. A 4×4 grid of textured cubes spins in place while the whole formation
rotates; an orbit camera flies around it. It exercises the full stack end to end:

- **app** — window + GPU device + input + the main loop (`App` / `Application` hooks).
- **scene + ecs** — a "formation" root node with 16 child cube nodes; each cube
  carries scene transforms plus a `renderer::MeshRenderer` (makes it draw itself)
  and a game-defined `Spin` component. The hierarchy propagates
  (`Scene::updateTransforms()`), so cubes inherit the formation's rotation on top of
  their own spin.
- **render + renderer** — a depth-tested, textured pipeline driven by a
  **ring-buffered per-frame camera uniform** (`viewProj`, re-uploaded every frame)
  and a **per-object push-constant** model matrix. The whole draw is one call:
  `renderer::renderMeshes(device, scene.world())` — no hand-wired per-cube loop.

No Vulkan or shader-compiler dependency at build or run time — the GLSL in
[`shaders/`](shaders) is cooked to SPIR-V offline by
[`zuki-shaderc`](../../tools/shader_compiler) and embedded as
[`src/sandbox_shaders.hpp`](src/sandbox_shaders.hpp).

## Build & run

Games are off by default; enable them at configure time:

```bash
cmake --preset debug -DZUKI_BUILD_GAMES=ON
cmake --build --preset debug --target zuki_sandbox
./build/debug/bin/zuki_sandbox
```

Needs a display and a Vulkan device (verified on an NVIDIA RTX 3060; clean under
ASan). Set `ZUKI_MAX_FRAMES=N` to auto-quit after N frames — handy for a
scripted smoke test:

```bash
ZUKI_MAX_FRAMES=90 ./build/debug/bin/zuki_sandbox
```

## Controls

| Key | Action |
| --- | --- |
| `A` / `D` or `←` / `→` | orbit left / right |
| `W` / `S` or `↑` / `↓` | zoom in / out |
| `Q` / `E` | raise / lower the camera |
| `Esc` | quit |

The camera also auto-orbits gently when idle.

## What it's for

Beyond being a demo, the sandbox is the integration test for the engine's public
API: if a Layer-0…3 change breaks the "make a game" path, this is where it shows.
It is also the real-GPU coverage for the [`renderer`](../../engine/modules/renderer)
module, which it dogfoods via `MeshRenderer` + `renderMeshes`.

# Zukiru — Project Structure

A scalable, modular directory layout for the **Zukiru** game engine.

- **Language / build:** C++20, CMake (targets-based, one `CMakeLists.txt` per module)
- **Architecture:** Hybrid — a data-oriented **ECS** core with an optional **scene-graph / GameObject** convenience layer on top
- **Dependencies:** vendored via a package manager (vcpkg or Conan; see `third_party/`)

The guiding rule: **the engine is a set of libraries, not a monolith.** The editor, tools, tests, and games are all *consumers* of those libraries. Nothing in `engine/` may depend on the editor, a specific game, or the tools.

---

## 1. Top-level layout

```
zukiru/
├── CMakeLists.txt              # Root: project(), options, add_subdirectory() of each area
├── CMakePresets.json           # Named configs (debug/release/asan, per-platform)
├── README.md
├── .gitignore
├── .clang-format               # Enforced code style
├── .clang-tidy                 # Static analysis rules
│
├── agents/                     # AI-agent context & engineering notes (this folder)
│
├── cmake/                      # Reusable CMake helpers & find modules
│   ├── ZukiruModule.cmake      # add_zukiru_module() wrapper (warnings, C++ std, install)
│   ├── CompilerWarnings.cmake
│   ├── Platform.cmake          # OS/arch detection, feature flags
│   └── Dependencies.cmake      # find_package / FetchContent glue
│
├── engine/                     # THE ENGINE — pure libraries, no app entry point
│   ├── CMakeLists.txt
│   ├── modules/                # See §2. Each is an independent library target
│   └── public/                 # (optional) umbrella "zukiru/zukiru.hpp" convenience header
│
├── editor/                     # Standalone editor app, links against engine modules
│   ├── CMakeLists.txt
│   ├── src/
│   └── resources/
│
├── tools/                      # Offline / CLI tools (asset cooker, shader compiler, etc.)
│   ├── CMakeLists.txt
│   ├── asset_cooker/
│   ├── shader_compiler/
│   └── pak_builder/
│
├── runtime/                    # Thin "player" executable that boots a game package
│   ├── CMakeLists.txt
│   └── src/main.cpp
│
├── games/                      # Sample games & test beds (each links the engine)
│   ├── sandbox/                # Dev playground for engine features
│   └── pong/                   # Minimal end-to-end sample
│
├── tests/                      # Cross-module integration tests (unit tests live in modules)
│   ├── CMakeLists.txt
│   └── integration/
│
├── benchmarks/                 # Micro & macro perf benchmarks
│
├── assets/                     # Shared/engine-default assets (fonts, editor icons, shaders)
│   ├── shaders/
│   ├── fonts/
│   └── textures/
│
├── docs/                       # Human-facing docs (architecture, ADRs, guides)
│   ├── architecture/
│   ├── adr/                    # Architecture Decision Records
│   └── guides/
│
├── scripts/                    # Build/CI/dev automation (setup, format, lint, package)
│
├── third_party/                # Vendored deps or package-manager manifests
│   ├── vcpkg.json              # (if using vcpkg) OR conanfile.txt
│   └── <submodules>/           # Header-only / patched libs kept in-tree
│
└── .github/ (or .gitlab/)      # CI workflows
    └── workflows/
```

---

## 2. Engine modules (`engine/modules/`)

Each module is a **separate CMake library target** named `zukiru::<module>`. Modules declare their dependencies explicitly and may **only depend on lower layers**. This dependency discipline is what keeps the engine scalable — it prevents the "everything includes everything" rot that kills large C++ projects.

### Layering (low → high; a module may only `#include` from its own layer or below)

```
Layer 0  foundation   core, math, memory, containers, platform, log
Layer 1  services     jobs, assets, filesystem, reflect, event, input
Layer 2  subsystems   ecs, render, audio, physics, animation, scene
Layer 3  gameplay     gameplay, scripting, ui, net
Layer 4  frameworks   app  (ties everything into a runnable loop)
```

### Module list

| Module | Layer | Responsibility |
|--------|-------|----------------|
| `core` | 0 | Types, assertions, `Result<T>`, string utils, time, config. The one module everyone depends on. |
| `math` | 0 | vec/mat/quat, transforms, geometry, SIMD helpers. |
| `memory` | 0 | Allocators (arena, pool, stack), smart handles, tracking. |
| `containers` | 0 | Engine-tuned data structures (sparse set, slot map, ring buffer). |
| `platform` | 0 | OS abstraction: windowing, threads, dynamic libs, clock, file I/O primitives. |
| `log` | 0 | Structured logging, sinks, channels. |
| `jobs` | 1 | Task/job system, fibers or thread pool, parallel-for. |
| `filesystem` | 1 | Virtual file system, mount points, path resolution. |
| `assets` | 1 | Async asset loading, handles, hot-reload, ref-counting, importers registry. |
| `reflect` | 1 | Runtime reflection / type registry — powers serialization & the editor. |
| `event` | 1 | Event bus / message dispatch. |
| `input` | 1 | Input mapping, devices, action bindings. |
| `ecs` | 2 | **Core of the engine.** Entities, components, systems, world, queries, archetypes. |
| `scene` | 2 | Hybrid layer: scene graph, `GameObject`/entity wrappers, transform hierarchy, prefabs, (de)serialization. Sits *on top of* `ecs`. |
| `render` | 2 | RHI abstraction (Vulkan/D3D12/Metal backends), render graph, materials, cameras. |
| `audio` | 2 | Mixer, sources, spatialization. |
| `physics` | 2 | Collision, rigid bodies (wraps a backend like Jolt/Bullet). |
| `animation` | 2 | Skeletal & property animation, blend trees. |
| `gameplay` | 3 | High-level gameplay building blocks built on ECS + scene. |
| `scripting` | 3 | Script VM binding (Lua/AngelScript/native), hot-reload. |
| `ui` | 3 | In-game UI (retained or immediate), layout, widgets. |
| `net` | 3 | Networking, replication, transport. |
| `app` | 4 | Application shell: main loop, subsystem init/shutdown order, plugin loading. |

### Anatomy of a single module

```
engine/modules/render/
├── CMakeLists.txt              # add_zukiru_module(render DEPENDS core math platform assets)
├── include/zukiru/render/      # PUBLIC headers — the module's API surface
│   ├── renderer.hpp
│   ├── material.hpp
│   └── render_graph.hpp
├── src/                        # PRIVATE implementation (+ private headers)
│   ├── renderer.cpp
│   ├── backend/
│   │   ├── vulkan/
│   │   └── d3d12/
│   └── internal/
├── tests/                      # Unit tests for THIS module (linked into the test runner)
│   └── material_test.cpp
└── README.md                   # What this module is, its deps, and any gotchas
```

**Convention:** public headers live under `include/zukiru/<module>/…` so every include reads `#include <zukiru/render/renderer.hpp>` — self-documenting and collision-free. `src/` is private and never installed. `target_include_directories(... PUBLIC include PRIVATE src)` enforces this at compile time.

---

## 3. CMake conventions

- **One target per module.** Use the `add_zukiru_module()` helper (`cmake/ZukiruModule.cmake`) so every module gets identical warning flags, C++ standard, sanitizer wiring, and install rules.
- **Namespaced ALIAS targets:** every library exports `zukiru::core`, `zukiru::render`, etc. Consumers link the alias, never a raw path.
- **`PUBLIC` vs `PRIVATE` link deps are law.** If module A's *public headers* include module B, link B `PUBLIC`; otherwise `PRIVATE`. This is how transitive dependencies stay correct and minimal.
- **No global `include_directories()`** — everything flows through `target_*` commands.
- **Options** (`ZUKIRU_BUILD_EDITOR`, `ZUKIRU_BUILD_TESTS`, `ZUKIRU_RENDER_BACKEND=vulkan`, `ZUKIRU_BUILD_SHARED`) live in the root `CMakeLists.txt` and gate `add_subdirectory()` calls.
- **Presets** (`CMakePresets.json`) give named `debug`, `release`, `asan`, `tsan` configurations so contributors and CI share one source of truth.

Example module CMake:

```cmake
add_zukiru_module(render
  PUBLIC_DEPS  core math assets
  PRIVATE_DEPS platform log
  BACKENDS     vulkan d3d12        # conditionally compiled subdirs
)
```

---

## 4. Why this scales

1. **Enforced layering** — the dependency table + `PUBLIC/PRIVATE` linking make illegal dependencies a *compile error*, not a code-review nag. The engine can grow to 30+ modules without turning into spaghetti.
2. **Independent buildability & testing** — each module builds and unit-tests in isolation, so CI can cache and parallelize, and a change to `audio` doesn't recompile `render`.
3. **Engine ≠ app** — because `engine/` has no `main()` and no editor/game dependencies, the same libraries power the editor, the shipping runtime, headless servers, and tests without duplication.
4. **Hybrid architecture is contained** — `ecs` knows nothing about the scene graph; `scene` is a thin, *optional* convenience layer on top. Teams that want raw ECS performance skip it; teams that want Unity-like ergonomics use it. Either way the data lives in one place.
5. **Clear extension points** — new subsystems drop into `engine/modules/`, new tools into `tools/`, new backends into a module's `src/backend/`. The top-level shape never changes.
6. **Tooling is first-class** — `reflect` + `assets` + `tools/asset_cooker` mean the editor and the offline pipeline share the same type info and importers, so there's no drift between "what the editor sees" and "what ships."

---

## 5. Conventions cheat-sheet

- **Namespace:** everything under `zuki::` (e.g. `zuki::render`, `zuki::ecs`). Module = nested namespace. **Exception:** the `core` module populates the root `zuki` namespace directly (it is the shared vocabulary — `zuki::i32`, `zuki::Result`, …); see `docs/adr/0002-core-root-namespace.md`.
- **Files:** `snake_case.hpp` / `snake_case.cpp`. Types `PascalCase`, functions/vars `camelCase` or `snake_case` — pick one in `.clang-format` and never argue about it again.
- **Public API** goes in `include/zukiru/<module>/`; anything in `src/` is private.
- **A new module is not "done"** until it has: a `CMakeLists.txt` using the helper, a `README.md`, at least one test, and an entry in the dependency table above.
- **Assets referenced by the engine core** ship in `assets/`; game-specific assets live under that game's folder in `games/`.

---

*Keep this document in sync when the module layout changes — it is the map future contributors (human and agent) navigate by.*

# Zukiru — TODO / Roadmap

Working task list for AI agents and humans. **Read this before starting work; update it after.**

## How to use this file
- Pick the **first unblocked, unchecked** task in the earliest milestone. Respect the layering in [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) — don't build a Layer-2 module before its Layer-0/1 deps exist.
- Status markers: `[ ]` todo · `[~]` in progress · `[x]` done · `[!]` blocked (add a `— blocked by:` note).
- When you start a task, mark it `[~]` and put your agent id + date. When you finish, mark `[x]`, add the date, and check that its [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) entry and any ADR exist.
- A module task is **not done** until: `CMakeLists.txt` (via `add_zukiru_module()`), a `README.md`, at least one test, and a dependency-table entry all exist (see structure doc §5).
- If you make a non-obvious decision, add an ADR under `docs/adr/` and, if relevant, log it in [CONTEXT.md](CONTEXT.md).
- Break big tasks into sub-tasks here rather than doing hidden work. Keep this file honest.

---

## Milestone 0 — Bootstrap the build & repo (do this first) — ✅ DONE 2026-07-03 (agent: claude-opus-4-8)
- [x] Choose package manager (vcpkg vs Conan) → write `docs/adr/0001-package-manager.md` — **vcpkg** (manifest mode; toolchain chainloaded when `VCPKG_ROOT` set; Catch2 via FetchContent fallback). ADR 0001 written. (2026-07-03)
- [x] Root `CMakeLists.txt`: `project(Zukiru CXX)`, C++20, options (`ZUKIRU_BUILD_EDITOR/TESTS/BENCHMARKS/TOOLS/GAMES/SHARED`, `ZUKIRU_RENDER_BACKEND`), guarded `add_subdirectory()` for each area (2026-07-03)
- [x] `CMakePresets.json`: `debug`, `release`, `asan`, `tsan` (asan/tsan gated off Windows); matching build + test presets. All four validated via `--list-presets`. (2026-07-03)
- [x] `cmake/ZukiruModule.cmake`: `add_zukiru_module()` (auto-glob src/, PUBLIC/PRIVATE deps, BACKENDS selection, header-only support, namespaced alias, install, auto test wiring via Catch2/CTest) (2026-07-03)
- [x] `cmake/CompilerWarnings.cmake` (`zukiru::warnings`), `cmake/Platform.cmake` (OS/arch + `zukiru::platform_flags`), `cmake/Dependencies.cmake` (vcpkg bootstrap + Catch2 acquisition) (2026-07-03)
- [x] `.clang-format` (Google-based, 4-space, col 100) + `.clang-tidy` (locks naming: types `PascalCase`, funcs/vars `camelCase`, private members `_` suffix, constants `k`-prefix, macros `ZUKI_` UPPER, namespace `zuki::`) (2026-07-03)
- [x] Fill `.gitignore` (build dirs, CMake/vcpkg scratch, cooked assets, IDE files) (2026-07-03)
- [x] Test framework wired in (**Catch2 v3.7.1**) + trivial passing test — `tests/smoke_test.cpp` (2 cases). Built with `debug` preset, **2/2 passed** via CTest. (2026-07-03)
- [x] CI workflow in `.github/workflows/ci.yml` — configure/build/test matrix on Ubuntu (gcc debug, gcc release, asan) (2026-07-03)
- [x] `README.md` at repo root (what Zukiru is, requirements, build/preset instructions, options table, how to add a module) (2026-07-03)

## Milestone 1 — Foundation (Layer 0)
- [x] `core` — types, assertions, `Result<T>`, string utils, time, config — **done 2026-07-03** (agent: claude-opus-4-8). Root `zuki` namespace (ADR 0002). Headers: types/assert/result/string_utils/time/config (+ umbrella). 37 unit tests, green in `debug` + `release` (warnings-as-errors). README + dep-table entry present.
- [x] `math` — vec/mat/quat, transforms, geometry, SIMD helpers — **done 2026-07-03** (agent: claude-opus-4-8). Header-only, `zuki::math` namespace, depends on `core`. RH / column-major / column-vector / clip-depth [0,1] conventions (documented in README). Headers: scalar/vec/mat/quat/transform/geometry (+ umbrella). 39 unit tests, green in `debug` + `release`. SIMD deferred (types are aligned & SIMD-ready). README + dep-table entry present.
- [ ] `log` — structured logging, sinks, channels
- [ ] `memory` — arena/pool/stack allocators, handles, tracking
- [ ] `containers` — sparse set, slot map, ring buffer
- [ ] `platform` — windowing, threads, dynamic libs, clock, file I/O primitives

## Milestone 2 — Services (Layer 1)
- [ ] `jobs` — task/job system, parallel-for
- [ ] `filesystem` — virtual FS, mount points, path resolution
- [ ] `reflect` — runtime type registry (powers serialization + editor)
- [ ] `event` — event bus / message dispatch
- [ ] `assets` — async loading, handles, hot-reload, importer registry
- [ ] `input` — device abstraction, action mapping

## Milestone 3 — Core subsystems (Layer 2)
- [ ] `ecs` — entities, components, systems, world, queries, archetypes (**engine core**)
- [ ] `scene` — scene graph / GameObject layer, transform hierarchy, prefabs, serialization (on top of `ecs`)
- [ ] `render` — RHI abstraction + first backend (Vulkan), render graph, materials, cameras
- [ ] `app` (Layer 4, but needed early) — main loop, subsystem init/shutdown order → **first triangle on screen**

## Milestone 4 — First playable & tooling
- [ ] `runtime/` — thin player executable that boots a game package
- [ ] `games/sandbox/` — dev playground exercising current features
- [ ] `tools/asset_cooker/` + `tools/shader_compiler/` — offline pipeline sharing `reflect`/`assets`
- [ ] `games/pong/` — minimal end-to-end sample (proves the loop works)

## Milestone 5 — Remaining subsystems
- [ ] `physics` (wrap Jolt/Bullet) · [ ] `audio` · [ ] `animation`
- [ ] `ui` · [ ] `scripting` · [ ] `net` · [ ] `gameplay`
- [ ] Additional render backends (`d3d12`, `metal`)

## Milestone 6 — Editor
- [ ] `editor/` shell (dockable panels, links engine modules)
- [ ] Scene hierarchy + inspector (driven by `reflect`)
- [ ] Asset browser, viewport, play-in-editor

---

## Cross-cutting / ongoing
- [ ] Keep [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) in sync with the actual module layout
- [ ] Each module ships with unit tests; add integration tests under `tests/integration/`
- [ ] Perf-sensitive modules get benchmarks under `benchmarks/`
- [ ] Record significant decisions as ADRs in `docs/adr/`

## Decision log (resolve these as they come up)
- [x] Package manager: **vcpkg** (manifest mode) — see `docs/adr/0001-package-manager.md` (2026-07-03)
- [ ] Scripting language: Lua vs AngelScript vs native-only
- [ ] Physics backend: Jolt vs Bullet vs custom
- [x] Namespace **confirmed `zuki::`** and locked in `.clang-tidy` (types `PascalCase`, funcs/vars `camelCase`, files `snake_case`; target/alias + include path use `zukiru::`/`zukiru/`) (2026-07-03)

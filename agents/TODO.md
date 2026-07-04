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
- [x] `.clang-format` (Google-based, 4-space, col 100) + `.clang-tidy` (locks naming: types `PascalCase`, funcs/vars `camelCase`, private members `_` suffix, constants `k`-prefix, macros `ZUKIRU_` UPPER, namespace `zukiru::`) (2026-07-03)
- [x] Fill `.gitignore` (build dirs, CMake/vcpkg scratch, cooked assets, IDE files) (2026-07-03)
- [x] Test framework wired in (**Catch2 v3.7.1**) + trivial passing test — `tests/smoke_test.cpp` (2 cases). Built with `debug` preset, **2/2 passed** via CTest. (2026-07-03)
- [x] CI workflow in `.github/workflows/ci.yml` — configure/build/test matrix on Ubuntu (gcc debug, gcc release, asan) (2026-07-03)
- [x] `README.md` at repo root (what Zukiru is, requirements, build/preset instructions, options table, how to add a module) (2026-07-03)

## Milestone 1 — Foundation (Layer 0)
- [x] `core` — types, assertions, `Result<T>`, string utils, time, config — **done 2026-07-03** (agent: claude-opus-4-8). Root `zukiru` namespace (ADR 0002). Headers: types/assert/result/string_utils/time/config (+ umbrella). 37 unit tests, green in `debug` + `release` (warnings-as-errors). README + dep-table entry present.
- [x] `math` — vec/mat/quat, transforms, geometry, SIMD helpers — **done 2026-07-03** (agent: claude-opus-4-8). Header-only, `zukiru::math` namespace, depends on `core`. RH / column-major / column-vector / clip-depth [0,1] conventions (documented in README). Headers: scalar/vec/mat/quat/transform/geometry (+ umbrella). 39 unit tests, green in `debug` + `release`. SIMD deferred (types are aligned & SIMD-ready). README + dep-table entry present.
- [x] `log` — structured logging, sinks, channels — **done 2026-07-03** (agent: claude-opus-4-8). `zukiru::log` namespace, depends on `core`. LogLevel (Trace..Critical/Off) + per-channel thresholds; `Logger` (thread-safe, pluggable sinks) + `defaultLogger()`; Console/File/Callback sinks; `ZUKIRU_LOG_*` macros with lazy `std::format` + compile-time floor (`kCompiledMinLevel`: Trace in debug, Info in release). 13 unit tests, green in `debug` + `release`. README + dep-table entry present.
- [x] `memory` — arena/pool/stack allocators, handles, tracking — **done 2026-07-03** (agent: claude-opus-4-8). `zukiru::memory` namespace, depends on `core`. `ArenaAllocator` (bump/reset), `StackAllocator` (LIFO markers), `PoolAllocator` (fixed-block free list), `Handle<Tag>` (index+generation), `MemoryTracker` (lock-free atomic counters), alignment helpers. Allocators return raw storage (own or borrow a buffer), move-only. 27 unit tests, green in `debug` + `release` + `asan` (validates raw pointer math). README + dep-table entry present.
- [x] `containers` — sparse set, slot map, ring buffer — **done 2026-07-03** (agent: claude-opus-4-8). Header-only, `zukiru::containers` namespace, depends on `core` + `memory` (slot map reuses `memory::Handle`). `SparseSet<T>` (dense storage, swap-remove), `SlotMap<T,Tag>` (generational handles, stale-detection, `std::optional` slots), `RingBuffer<T>` (FIFO push/pop/pushOverwrite). 17 unit tests, green in `debug` + `release` + `asan`. README + dep-table entry present. NOTE: dropped `-Wnull-dereference` from the warning set (GCC -O3 false positives; UBSan covers it) — see `cmake/CompilerWarnings.cmake`. Also marked Catch2 includes SYSTEM so third-party headers aren't policed by `-Werror`.
- [~] `platform` — windowing, threads, dynamic libs, clock, file I/O primitives — **mostly done 2026-07-03** (agent: claude-opus-4-8). `zukiru::platform` namespace, depends on `core` (+ `Threads::Threads`, `${CMAKE_DL_LIBS}`). Done: clock (sleep/perf-counter/wall-time), thread (concurrency/id/name/yield), `DynamicLibrary` (dlopen/LoadLibrary), file I/O (read/write/exists/size, Result-based). 23 unit tests, green in `debug` + `release` + `asan`. README + dep-table entry present.
  - [ ] **Windowing backend** — interface (`window.hpp` `Window`/`WindowConfig`) shipped; `createWindow()` returns an error until a concrete backend lands. Blocked on: backend choice (GLFW recommended) + vcpkg dep + a display for testing. See `docs/adr/0003-windowing-backend.md`. Implement `platform/src/backend/glfw/` + windowed integration tests.
  - [ ] Verify Windows build path (thread naming via `SetThreadDescription`, `LoadLibrary`) once a Windows CI runner exists.

## Milestone 2 — Services (Layer 1)
- [x] `jobs` — task/job system, parallel-for — **done 2026-07-03** (agent: claude-opus-4-8). `zukiru::jobs` namespace, depends on `core` (public) + `platform` (private). `JobSystem` thread pool: `submit` (fire-and-forget), `async` (std::future), `parallelFor` (chunked + auto-chunk), `waitIdle`. Help-while-waiting (blocked threads run pending tasks) prevents nested-dispatch deadlock and idle cores. 9 unit tests, green in `debug` + `release` + `asan` + **`tsan` (no data races)**. README + dep-table entry present.
- [x] `filesystem` — virtual FS, mount points, path resolution — **done 2026-07-03** (agent: claude-opus-4-8). `zukiru::filesystem` namespace, depends on `core` (public) + `platform` (private). `path::` virtual-path utils (normalize/join/filename/extension/stem/parentPath; `..` clamped at root). `FileSystem` VFS: `mount`/`unmount` (writable flag), longest-prefix `resolve`, `readFile`/`readFileBinary`/`writeFile`/`exists`/`fileSize`. No mount escape (traversal-safe). Packed-archive/async backends layer on later. 19 unit tests (real temp dirs), green in `debug` + `release` + `asan`. README + dep-table entry present.
- [x] `reflect` — runtime type registry (powers serialization + editor) — **done 2026-07-03** (agent: claude-opus-4-8). Header-only, `zukiru::reflect` namespace, depends on `core`. `Registry` (find by T/id/name, `global()` singleton, movable), fluent `TypeBuilder<T>.field(name, &T::member)`, `TypeInfo` + `Field` with type-erased member-pointer accessors (`get`/`set`/`getRef`/`raw`/`is<T>`), nested-type resolution via the registry. RTTI-free type ids. MVP (types + fields); enums/methods/attributes extend later. 7 unit tests incl. a reflection-driven serialization demo, green in `debug` + `release` + `asan`. README + dep-table entry present.
- [x] `event` — event bus / message dispatch — **done 2026-07-03** (agent: claude-opus-4-8). Header-only, `zukiru::event` namespace, depends on `core`. `EventBus` type-safe pub/sub (RTTI-free type ids): `subscribe<E>` returns RAII `Subscription`, `publish<E>` (synchronous, snapshot dispatch — safe to (un)subscribe mid-handler), `enqueue<E>`/`dispatchQueued` (deferred). Not thread-safe by design (one per thread). 10 unit tests, green in `debug` + `release` + `asan`. README + dep-table entry present.
- [x] `assets` — async loading, handles, hot-reload, importer registry — **done 2026-07-04** (agent: claude-opus-4-8). `zukiru::assets` namespace. PUBLIC deps `core` + `filesystem` + `jobs` (the last two appear forward-declared in the `AssetManager` constructor). `AssetId` (FNV-1a of the normalized virtual path). `AssetHandle<T>`: cheap, copyable, **ref-counted** typed reference — `get()`/`state()`/`isLoaded()`/`error()`/`version()` (bumps on each successful (re)load). `AssetManager`: `registerImporter<T>({exts}, fn)` (case-insensitive, dot-optional), `load`/`loadAsync` (async via the job pool; cached & de-duped by id), `find` (cache-only), `reload`/`reloadAll` (re-import **in place**, swap content, bump version; failed reload keeps last-good content), `unload` + `garbageCollect` (evicts records with no live handle → ref-counted unload). All filesystem/jobs use is confined to the .cpp behind type-erased helpers, so public headers stay light. Thread-safe (mutex-guarded map; records publish via `state` release/acquire + `std::atomic<std::shared_ptr>`). 12 unit tests (real temp-dir files through a mounted VFS, live `JobSystem` for async), green in `debug` + `release` + `asan` + **`tsan` (no data races)**. README + dep-table entry present. Deferred (documented in README): asset dependency graphs, packed-archive backend, auto file-watcher, `reflect`-driven data (de)serialization.

> **Milestone 2 (Layer 1) complete** — jobs, filesystem, reflect, event, assets, input all done. Next: Milestone 3 (Layer 2), starting with `ecs`.
- [x] `input` — device abstraction, action mapping — **done 2026-07-04** (agent: claude-opus-4-8). Header-only, `zukiru::input` namespace, depends on `core`. `key_code.hpp`: dense `Key`/`MouseButton`/`GamepadButton`/`GamepadAxis` enums (ending in `Count` for array-indexed state) + `KeyMods` bit flags. `InputState` is a *sink*, not a poller — a backend/test pushes raw events via `on*()`, game code queries it; `beginFrame()` rolls current→previous for one-frame edge detection (`keyPressed`/`keyReleased`, mouse delta, accumulated scroll). `ActionMap`: rebindable named button actions (fire if *any* `ButtonBinding` fires; optional modifier gating via `withMods`) and axis actions (`AxisBinding` gamepad-analog with scale/deadzone, or digital key pair; summed + clamped to [-1,1]). Stays independent of the deferred windowing backend (ADR 0003). MVP: single gamepad, no text/IME, no rebind-serialization yet. 11 unit tests, green in `debug` + `release` + `asan`. README + dep-table entry present.

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
- [x] Namespace **confirmed `zukiru::`** and locked in `.clang-tidy` (types `PascalCase`, funcs/vars `camelCase`, files `snake_case`; target/alias + include path use `zukiru::`/`zukiru/`) (2026-07-03)

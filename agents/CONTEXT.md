# Zukiru — Agent Context

Notes for AI agents (and humans) working on this repo. Read this first.

## What Zukiru is
A game engine, currently greenfield (started 2026-07-03 — the repo was empty apart from this `agents/` folder and an empty `.gitignore`).

## Core technical decisions (locked)
- **Language / build:** C++20 with CMake (targets-based, one `CMakeLists.txt` per module).
- **Architecture:** **Hybrid** — a data-oriented ECS core (`ecs` module) with an optional scene-graph / GameObject convenience layer (`scene` module) built on top. `ecs` must never depend on `scene`.
- **Dependencies:** **vcpkg** (manifest mode, `vcpkg.json`) — chosen 2026-07-03, recorded in `docs/adr/0001-package-manager.md`. Toolchain auto-chainloads when `VCPKG_ROOT` is set; Catch2 is fetched via CMake FetchContent so a clean checkout builds without vcpkg.
- **License:** **`MIT OR Apache-2.0`** (dual, user's choice) — chosen 2026-07-06, recorded in `docs/adr/0007-license.md`. Files `LICENSE-MIT` + `LICENSE-APACHE` at repo root; contributions are inbound under the same dual license. Copyright holder is "Zukiru contributors". Effectively permanent — don't relicense without all contributors' consent.

## The one rule that matters most
The engine is a **set of layered libraries**, not a monolith. Enforced module layering (see [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) §2) means a module may only depend on its own layer or below. Illegal dependencies should be a compile error, not a review comment. `engine/` never depends on the editor, tools, or any specific game.

## Where things live
Full map in [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md). Quick version:
- `engine/modules/` — the engine, as independent `zukiru::<module>` library targets.
- `editor/`, `tools/`, `runtime/`, `games/` — *consumers* of the engine.
- `docs/adr/` — record every significant decision as an ADR.

## Build bootstrap (Milestone 0 — done 2026-07-03)
- Build works today: `cmake --preset debug && cmake --build --preset debug && ctest --preset debug` (smoke test green).
- Presets: `debug`, `release` (warnings-as-errors), `asan`, `tsan` (asan/tsan skipped on Windows).
- Modules are created via `add_zukiru_module()` (`cmake/ZukiruModule.cmake`) and **auto-discovered** from `engine/modules/*/CMakeLists.txt` — no root edits to add one.
- Naming is locked in `.clang-tidy`; test framework is Catch2 v3.7.1.

## Open decisions (not yet made)
- Render backend priority (Vulkan assumed first; `ZUKIRU_RENDER_BACKEND` defaults to `vulkan`).
- Scripting language (Lua / AngelScript / native-only).
- Physics backend (Jolt / Bullet / custom).
- ~~Package manager~~ → vcpkg (ADR 0001). ~~Namespace~~ → `zukiru::` confirmed & locked.

## For agents
- Keep `PROJECT_STRUCTURE.md` in sync with reality whenever the module layout changes.
- When you make a non-obvious engineering decision, drop an ADR in `docs/adr/` and, if it changes how future work is done, note it here.

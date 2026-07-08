# Zuki

A modular, data-oriented **C++20 game engine**. Zuki is built as a set of
layered libraries — not a monolith — with a data-oriented **ECS** core and an
optional **scene-graph / GameObject** convenience layer on top. The editor,
tools, runtime and sample games are all *consumers* of the engine libraries.

> Status: **greenfield / bootstrapping** (started 2026-07-03). The build system,
> module scaffolding and CI are in place; engine modules land next
> (see [`agents/TODO.md`](agents/TODO.md)).

## Layout

| Path | What |
|------|------|
| `engine/modules/` | The engine, as independent `zuki::<module>` library targets |
| `editor/` `tools/` `runtime/` `games/` | Consumers of the engine |
| `cmake/` | Shared CMake helpers (`add_zuki_module()`, warnings, platform) |
| `tests/` | Integration + build-pipeline tests (unit tests live in each module) |
| `docs/adr/` | Architecture Decision Records |
| `agents/` | Engineering context & roadmap for humans and AI agents |

Full map: [`agents/PROJECT_STRUCTURE.md`](agents/PROJECT_STRUCTURE.md).

## Requirements

- **CMake ≥ 3.24** and a build generator (**Ninja** recommended)
- A **C++20** compiler: GCC 12+, Clang 15+, or MSVC 19.3x (VS 2022)
- *(optional)* [**vcpkg**](https://vcpkg.io) for third-party dependencies —
  Zuki's standard package manager (see
  [ADR 0001](docs/adr/0001-package-manager.md)). Not required to bootstrap: the
  test framework (Catch2) is fetched automatically when vcpkg is absent.

## Build

Zuki uses CMake presets. From the repo root:

```bash
cmake --preset debug        # configure
cmake --build --preset debug
ctest --preset debug        # run tests
```

Available presets: `debug`, `release`, `asan` (Address+UB sanitizers),
`tsan` (Thread sanitizer). To use vcpkg for dependencies, set `VCPKG_ROOT` in
your environment — the root `CMakeLists.txt` chainloads its toolchain
automatically.

### Build options

| Option | Default | Meaning |
|--------|---------|---------|
| `ZUKI_BUILD_TESTS` | `ON` | Build unit + integration tests |
| `ZUKI_BUILD_EDITOR` | `OFF` | Build the editor application |
| `ZUKI_BUILD_TOOLS` | `OFF` | Build offline tools |
| `ZUKI_BUILD_GAMES` | `OFF` | Build sample games |
| `ZUKI_BUILD_BENCHMARKS` | `OFF` | Build performance benchmarks |
| `ZUKI_BUILD_SHARED` | `OFF` | Build engine modules as shared libraries |
| `ZUKI_RENDER_BACKEND` | `vulkan` | Primary render backend (`vulkan`/`d3d12`/`metal`) |
| `ZUKI_WARNINGS_AS_ERRORS` | `OFF` | Treat warnings as errors (on in `release`) |

## Adding an engine module

Create `engine/modules/<name>/` with `include/zuki/<name>/`, `src/`,
`tests/`, a `README.md`, and a `CMakeLists.txt` of the form:

```cmake
add_zuki_module(<name>
  PUBLIC_DEPS  core math       # deps referenced by this module's public headers
  PRIVATE_DEPS platform log    # deps used only in src/
)
```

Modules are auto-discovered — no root edits needed. A module isn't "done" until
it has a CMake target (via the helper), a README, at least one test, and an
entry in the dependency table in
[`agents/PROJECT_STRUCTURE.md`](agents/PROJECT_STRUCTURE.md). Code style is
enforced by `.clang-format` / `.clang-tidy`: types `PascalCase`, functions/vars
`camelCase`, files `snake_case`, namespace `zuki::`.

## License

Zuki is dual-licensed under either of

- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE))
- MIT license ([LICENSE-MIT](LICENSE-MIT))

at your option. This is the same permissive dual-license used across much of the
Rust/game-engine ecosystem: you may use Zuki — including in closed-source and
commercial games — under whichever of the two licenses you prefer. See
[ADR 0007](docs/adr/0007-license.md) for the rationale.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in Zuki by you, as defined in the Apache-2.0 license, shall be
dual-licensed as above, without any additional terms or conditions.

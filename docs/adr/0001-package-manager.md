# ADR 0001 — Package manager: vcpkg

- **Status:** Accepted
- **Date:** 2026-07-03
- **Deciders:** Zuki build/bootstrap
- **Supersedes:** —

## Context

Zuki is a C++20, CMake, targets-based game engine (see
[PROJECT_STRUCTURE.md](../../agents/PROJECT_STRUCTURE.md)). It will pull in a
growing set of third-party libraries (math/SIMD helpers, a windowing/backend
layer, a Vulkan loader, a physics backend, a scripting VM, test frameworks,
etc.). We need one dependency-acquisition mechanism that:

1. Integrates cleanly with a modern, presets-driven CMake build.
2. Pins reproducible versions across contributors and CI.
3. Has a large catalog covering the libraries a game engine typically needs.
4. Works on the three target desktop platforms (Linux, Windows, macOS).

The two realistic candidates called out in the roadmap were **vcpkg** and
**Conan**.

## Decision

**Use [vcpkg](https://vcpkg.io) in _manifest mode_** as the standard package
manager for Zuki.

- Dependencies are declared in [`vcpkg.json`](../../vcpkg.json) at the repo root
  (manifest mode), with versions pinned via `builtin-baseline` /
  `overrides` as the tree stabilizes.
- vcpkg integrates through its CMake **toolchain file**. Rather than hard-coding
  that toolchain into `CMakePresets.json` (which would break checkouts that
  don't have vcpkg), the root `CMakeLists.txt` **chainloads it automatically**
  when the `VCPKG_ROOT` environment variable is set and no toolchain was
  supplied. See [`cmake/Dependencies.cmake`](../../cmake/Dependencies.cmake).
- The build must also configure **without** vcpkg present. Bootstrap-critical
  dependencies (currently just the **Catch2** test framework) are obtained via
  CMake `FetchContent` so the pipeline is provable on a clean machine. As the
  engine grows, heavier deps move into `vcpkg.json`; `FetchContent` stays the
  fallback for anything not yet ported or when `VCPKG_ROOT` is unset.

## Consequences

**Positive**
- First-class CMake integration; contributors run the same `cmake --preset …`
  regardless of whether deps come from vcpkg or FetchContent.
- Large, well-maintained port catalog; manifest mode gives reproducible,
  version-pinned installs and per-project isolation.
- No Python runtime requirement in the build (contrast with Conan).
- CI can cache the vcpkg binary/package cache for fast rebuilds.

**Negative / trade-offs**
- vcpkg's version-pinning ergonomics are less rich than Conan's profiles/lockfiles;
  we accept `builtin-baseline` + `overrides` for now.
- Two acquisition paths (vcpkg manifest + FetchContent) exist during bootstrap.
  This is deliberate and temporary-ish: it keeps a fresh clone buildable. Keep
  the list of FetchContent'd deps small and documented in `Dependencies.cmake`.

## Alternatives considered

- **Conan** — excellent versioning/profiles and lockfiles, widely used in
  studios. Rejected for now mainly to avoid the Python toolchain dependency in
  the build and because vcpkg's CMake toolchain integration is slightly more
  turnkey for a presets-based workflow. Revisit if cross-compilation or complex
  per-config dependency graphs make Conan profiles compelling.
- **FetchContent only** — simplest to bootstrap, but pulls and builds every dep
  from source with weak version governance and no binary caching. Kept only as
  the bootstrap fallback, not as the primary mechanism.

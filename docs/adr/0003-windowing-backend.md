# ADR 0003 — Windowing backend (deferred)

- **Status:** Proposed / deferred
- **Date:** 2026-07-03
- **Deciders:** Zukiru foundation work (Milestone 1, `platform`)
- **Related:** `engine/modules/platform/include/zukiru/platform/window.hpp`

## Context

The `platform` module defines an abstract `Window` interface (see
`window.hpp`), but a **concrete windowing backend is not yet implemented**.
Windowing is fundamentally different from the module's other primitives:

- It requires a **third-party dependency** (GLFW or SDL) or a large amount of
  per-OS native code (Win32 / X11 / Wayland / Cocoa).
- It cannot be **built or tested headlessly** — there is no display in CI or the
  current dev environment, and the windowing dependency isn't vendored yet
  (vcpkg isn't installed; see ADR 0001).

Rather than block the whole foundation layer on this, the interface ships now so
`render` and `app` can be designed against it, and the concrete backend is
tracked as a follow-up.

## Decision

**Defer the backend choice.** Ship `Window`/`WindowConfig` as an interface;
`createWindow()` returns an `Error` until a backend lands. When we implement one,
the leading candidates are:

- **GLFW** (recommended starting point) — small, permissively licensed, exactly
  the windowing + input + Vulkan-surface surface an engine needs; trivial to
  vendor via vcpkg. Best fit given Vulkan is the first render backend (ADR-era
  assumption).
- **SDL** — broader scope (audio, gamepads, etc.), heavier; useful if we later
  want its subsystems, but overlaps with modules we intend to own.
- **Native per-OS** — maximum control, no deps, but the most code and the most
  maintenance; revisit only if a dependency becomes untenable.

## Consequences

- Downstream modules can compile/link against the windowing API immediately.
- Anything that actually opens a window fails fast with a clear error until the
  backend is implemented.
- **Follow-up work** (tracked in `agents/TODO.md`): pick GLFW (or alternative),
  add it to `vcpkg.json`, implement a `GlfwWindow : Window` backend under
  `platform/src/backend/glfw/`, and add windowed integration tests gated behind
  a display being available.

# ADR 0002 — The `core` module populates the root `zukiru` namespace

- **Status:** Accepted
- **Date:** 2026-07-03
- **Deciders:** Zukiru foundation work (Milestone 1)
- **Related:** [PROJECT_STRUCTURE.md §5](../../agents/PROJECT_STRUCTURE.md)

## Context

The project convention (PROJECT_STRUCTURE.md §5) is: *"everything under `zukiru::`
… Module = nested namespace (e.g. `zukiru::render`, `zukiru::ecs`)."*

`core` is a special module: it defines the **shared vocabulary** of the whole
engine — fixed-width integer aliases (`i32`, `u64`, `f32`, …), `Result<T>`,
`Error`, assertion macros, `Unit`. These names appear in the signature of nearly
every function in every other module. Forcing `zukiru::core::i32` (or requiring a
`using namespace zukiru::core;` in every file) everywhere would be noisy and
un-idiomatic; most engines keep this vocabulary at the root.

## Decision

**The `core` module places its public API directly in the root `zukiru`
namespace**, not in a `zukiru::core` sub-namespace. So it is `zukiru::i32`,
`zukiru::Result`, `zukiru::Error`, etc.

- Include paths are unchanged and still self-documenting:
  `#include <zukiru/core/types.hpp>`.
- The CMake target/alias is unchanged: `zukiru::core`.
- Purely internal helpers still live in `zukiru::detail`.
- **Every other module keeps using its own nested namespace** (`zukiru::render`,
  `zukiru::ecs`, …). `core` is the single, deliberate exception because it *is* the
  root vocabulary.

## Consequences

- **Positive:** Call sites read naturally (`zukiru::Result<Foo> load();`), matching
  how foundational types are used across the codebase; no per-file `using`.
- **Negative:** One documented exception to the "module = nested namespace" rule.
  Mitigated by this ADR and a note in the `core` README so future contributors
  (human or agent) don't "fix" it back into `zukiru::core`.
- Names in `core` must stay genuinely foundational and collision-resistant to
  justify occupying the root namespace. Anything module-specific does **not**
  belong in `core`.

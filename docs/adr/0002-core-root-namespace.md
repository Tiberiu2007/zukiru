# ADR 0002 — The `core` module populates the root `zuki` namespace

- **Status:** Accepted
- **Date:** 2026-07-03
- **Deciders:** Zuki foundation work (Milestone 1)
- **Related:** [PROJECT_STRUCTURE.md §5](../../agents/PROJECT_STRUCTURE.md)

## Context

The project convention (PROJECT_STRUCTURE.md §5) is: *"everything under `zuki::`
… Module = nested namespace (e.g. `zuki::render`, `zuki::ecs`)."*

`core` is a special module: it defines the **shared vocabulary** of the whole
engine — fixed-width integer aliases (`i32`, `u64`, `f32`, …), `Result<T>`,
`Error`, assertion macros, `Unit`. These names appear in the signature of nearly
every function in every other module. Forcing `zuki::core::i32` (or requiring a
`using namespace zuki::core;` in every file) everywhere would be noisy and
un-idiomatic; most engines keep this vocabulary at the root.

## Decision

**The `core` module places its public API directly in the root `zuki`
namespace**, not in a `zuki::core` sub-namespace. So it is `zuki::i32`,
`zuki::Result`, `zuki::Error`, etc.

- Include paths are unchanged and still self-documenting:
  `#include <zuki/core/types.hpp>`.
- The CMake target/alias is unchanged: `zuki::core`.
- Purely internal helpers still live in `zuki::detail`.
- **Every other module keeps using its own nested namespace** (`zuki::render`,
  `zuki::ecs`, …). `core` is the single, deliberate exception because it *is* the
  root vocabulary.

## Consequences

- **Positive:** Call sites read naturally (`zuki::Result<Foo> load();`), matching
  how foundational types are used across the codebase; no per-file `using`.
- **Negative:** One documented exception to the "module = nested namespace" rule.
  Mitigated by this ADR and a note in the `core` README so future contributors
  (human or agent) don't "fix" it back into `zuki::core`.
- Names in `core` must stay genuinely foundational and collision-resistant to
  justify occupying the root namespace. Anything module-specific does **not**
  belong in `core`.

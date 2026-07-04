# ADR 0004 — ECS storage architecture: archetypes (SoA)

- **Status:** Accepted
- **Date:** 2026-07-04
- **Deciders:** claude-opus-4-8

## Context

`ecs` is the engine core (Layer 2): every gameplay-facing subsystem (`scene`,
`render`, gameplay) sits on top of it, so its storage model sets the performance
ceiling and the API shape for the whole engine. The dominant operation is the
**query**: "for every entity that has components A, B, C, do X" — run every frame,
over many thousands of entities. The two mainstream layouts are:

1. **Archetype / SoA** — group entities by their exact component set; each set is
   an "archetype" storing its components in parallel, contiguous column arrays.
   (Unity DOTS, Flecs, EnTT groups.)
2. **Sparse set per component** — one dense array per component type, indexed via
   a sparse map from entity id. (EnTT's default.)
3. **Array-of-structs "big table"** — one row per entity with optional columns;
   simple but wastes cache on absent components and on queries.

## Decision

Use an **archetype-based, struct-of-arrays** layout.

- An `Archetype` owns one contiguous `Column` (aligned raw buffer) per component
  type plus a parallel `entities` array. All entities with the identical component
  set live there.
- A query for `<A, B>` walks only the archetypes that contain both and iterates
  their columns directly — a tight, branch-free, cache-friendly loop with no
  per-entity "does it have this?" test.
- Adding/removing a component **moves** the entity's row to the archetype for the
  new set (relocating shared components by move-construction). Structural changes
  are therefore more expensive than iteration — the right trade, since iteration
  is the hot path and structural changes are comparatively rare.
- Rows are **packed**: removal swaps the last row into the hole (O(1)); the
  archetype reports the relocated entity so the World can fix its recorded row.
- Components are stored **type-erased** in the columns, keyed by a RTTI-free
  `ComponentId`, with per-type `size`/`alignment` and move/destruct function
  pointers (`ComponentInfo`). This keeps the archetype/query machinery in one
  non-template `.cpp` while the `World` public API stays templated and ergonomic.

## Consequences

- **Good:** best-in-class query iteration (contiguous SoA, no indirection);
  natural fit for a future job-system-parallelized `each`; archetypes are shared
  by set regardless of add-order, so the archetype graph stays small.
- **Cost:** add/remove component and create-with-components pay a row move between
  archetypes. Entities that churn their component set frequently are the
  anti-pattern; the fix (deferred/command-buffer structural changes) can layer on
  later without changing storage.
- **Pointer stability:** a component reference/pointer from `get()` is invalidated
  by any structural change to that entity (it moves) or column growth. Documented;
  callers re-fetch. Entity handles stay stable (generational).
- **Not thread-safe by design (yet):** iteration and structural mutation touch the
  same storage. `each` is single-threaded for now; parallel queries over disjoint
  archetypes are a later, additive step.

## Alternatives rejected

- **Sparse-set-per-component:** simpler structural changes and stable component
  storage, but multi-component queries hop between per-component arrays (worse
  locality) and need an intersection test per entity. Iteration is the priority,
  so archetypes win.
- **Big-table:** simplest, but wastes cache and scans absent entities. Rejected.

## Scope of the initial implementation

MVP: entities (generational handles), components, archetype storage with growth
and swap-remove, add/remove/get/has, and `each<...>` queries (with an optional
`Entity` first parameter and `const` components for read-only access). Deferred:
systems scheduler, deferred/command-buffer structural changes, parallel queries,
relationships, and change-detection. These extend the API without altering the
storage decision above.

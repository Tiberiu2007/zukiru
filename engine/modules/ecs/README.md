# ecs

**Layer 2 — the engine core.** An archetype-based entity/component/system
`World`: entities are lightweight generational handles, components are plain
structs stored struct-of-arrays, and queries are tight loops over contiguous
columns. Everything gameplay-facing (`scene`, `render`, systems) is built on this.

Depends only on [`core`](../core). Namespace `zuki::ecs`. The storage decision
is recorded in [ADR 0004](../../../docs/adr/0004-ecs-storage-architecture.md).

## Entities and components

```cpp
#include <zuki/ecs/ecs.hpp>
using namespace zuki::ecs;

struct Position { f32 x, y; };
struct Velocity { f32 x, y; };

World world;
Entity e = world.create(Position{0, 0}, Velocity{1, 2});  // create with components

world.add(e, Name{"hero"});     // attach (or overwrite) a component
world.has<Velocity>(e);         // true
world.get<Position>(e)->x;      // pointer, or nullptr if absent/dead
world.remove<Velocity>(e);
world.destroy(e);
```

`Entity` is an index + generation. Destroying an entity recycles its index but
bumps the generation, so a stale handle is detected by `isAlive()` and every
query — no dangling.

Components need only be **move-constructible and destructible** (they get relocated
between archetypes). No base class, no registration.

## Queries

```cpp
// Runs for every entity that has all listed components.
world.each<Position, const Velocity>([](Position& p, const Velocity& v) {
    p.x += v.x;
    p.y += v.y;
});

// Take the Entity too, and use `const` for read-only access:
world.each<const Position>([](Entity e, const Position& p) { /* ... */ });
```

A query visits only the archetypes that contain the requested set and iterates
their columns directly — no per-entity "does it have this?" check. Mark a
component `const` in the list for read-only access.

## How it's laid out

Each unique component **set** is an `Archetype` holding one contiguous `Column`
per component. Adding/removing a component **moves** the entity to another
archetype (relocating shared components); removal within an archetype swap-fills
the hole with the last row (O(1)). This makes iteration the fast path and
structural change the slower one — the right trade for a per-frame query workload.
See the ADR for the full rationale and rejected alternatives.

### Gotchas

- A pointer/reference from `get()` (or a component ref in `each`) is **invalidated
  by any structural change** to that entity (it moves archetypes) or by column
  growth. Re-fetch after structural edits. Entity handles stay valid.
- **Not thread-safe**, and you must not create/destroy entities or add/remove
  components *while* iterating an `each()` — that restructures the storage being
  walked. (Deferred/command-buffer structural changes will lift this later.)

## Scope

MVP: entities, components, archetype storage (growth + swap-remove), add / remove
/ get / has, and `each<...>` queries. Deferred (additive, no storage change):
a systems scheduler, deferred structural changes, parallel queries, relationships,
and change-detection — see the ADR.

## Tests

```bash
ctest --preset debug -R '^ecs\.'
```

10 unit tests covering generational reuse, archetype transitions, swap-remove,
column growth past capacity (200 entities), destructor accounting, and query
iteration/mutation. Run under `--preset asan` (validates the raw column
placement-new / aligned allocation / relocation).

## Dependencies

`core` (Layer 2). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).

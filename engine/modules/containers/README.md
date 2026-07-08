# containers

**Layer 0 — foundation.** Engine-tuned data structures. Header-only. Depends on
[`core`](../core) and [`memory`](../memory) (the slot map reuses
`memory::Handle`). Namespace `zuki::containers`.

## What's here

| Header | Type | Use it for |
|--------|------|-----------|
| `<zuki/containers/sparse_set.hpp>` | `SparseSet<T>` | Dense u32-keyed storage with O(1) insert/remove/lookup and cache-friendly iteration. The backbone of ECS component pools. |
| `<zuki/containers/slot_map.hpp>` | `SlotMap<T, Tag>` | Stable, generational handles to values — safe weak references that detect use-after-remove. |
| `<zuki/containers/ring_buffer.hpp>` | `RingBuffer<T>` | Fixed-capacity circular FIFO (bounded queues, history, producer/consumer). |
| `<zuki/containers/containers.hpp>` | — | Umbrella header. |

## SparseSet

Maps small, dense-ish `u32` keys to values via a sparse→dense indirection.
Values are packed contiguously (great for iteration); removal is a swap-with-last
so **iteration order is not stable**. The sparse array grows to `maxKey + 1`, so
keys should be small integers, not hashes.

```cpp
SparseSet<Transform> transforms;
transforms.insert(entityId, Transform{...});
if (Transform* t = transforms.get(entityId)) { /* ... */ }
for (Transform& t : transforms) { /* dense iteration */ }
```

## SlotMap

`insert()` returns a `Handle` (index + generation). Removing an element bumps the
slot's generation, so any handle to the old occupant becomes stale and is
rejected by `get()`/`contains()` — no dangling pointers even as slots are reused.
Pass a phantom `Tag` for type-safe handles across resource kinds.

```cpp
SlotMap<Mesh, struct MeshTag> meshes;
auto h = meshes.insert(loadMesh());
Mesh* m = meshes.get(h);   // nullptr if h is stale
meshes.remove(h);
meshes.forEach([](auto handle, Mesh& mesh){ /* live elements only */ });
```

## RingBuffer

Fixed-capacity FIFO. `push()` fails when full; `pushOverwrite()` drops the oldest
element to make room. `pop()` returns `std::optional<T>`.

```cpp
RingBuffer<Event> events(256);
events.pushOverwrite(evt);       // bounded history, never blocks
while (auto e = events.pop()) { handle(*e); }
```

## Notes

`SlotMap` and `RingBuffer` store elements in `std::optional<T>` so slots are
constructed/destroyed on insert/remove (no default-construction requirement, no
lingering moved-from values).

## Tests

```bash
ctest --preset debug -R '^containers\.'
```

Also exercised under `--preset asan`.

## Dependencies

`core`, `memory` (Layer 0). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).

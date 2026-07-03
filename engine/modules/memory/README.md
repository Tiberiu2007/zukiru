# memory

**Layer 0 — foundation.** Custom allocators, resource handles, and allocation
tracking. Depends only on [`core`](../core). Namespace `zukiru::memory`.

Games live and die by memory layout. These allocators trade the generality of
`new`/`delete` for speed and predictability: contiguous storage, no
fragmentation, and O(1) (often O(0)) frees.

## What's here

| Header | Provides |
|--------|----------|
| `<zukiru/memory/alignment.hpp>` | `alignUp`/`alignDown`/`isAligned`/`isPowerOfTwo`, `alignPointer`, `kDefaultAlignment`. |
| `<zukiru/memory/arena_allocator.hpp>` | `ArenaAllocator` — linear "bump" allocator; `reset()` frees everything at once. |
| `<zukiru/memory/stack_allocator.hpp>` | `StackAllocator` — linear with `mark()`/`freeToMarker()` LIFO rollback. |
| `<zukiru/memory/pool_allocator.hpp>` | `PoolAllocator` — fixed-size blocks, O(1) `allocate`/`free`, zero fragmentation. |
| `<zukiru/memory/handle.hpp>` | `Handle<Tag>` — index + generation, type-safe, detects stale references. |
| `<zukiru/memory/tracking.hpp>` | `MemoryTracker` — lock-free live/peak/total byte & allocation counters. |
| `<zukiru/memory/memory.hpp>` | Umbrella header. |

## Choosing an allocator

- **Arena** — transient, same-lifetime data (a frame, a load pass). Allocate
  freely, then `reset()`. No per-object free.
- **Stack** — nested scopes with LIFO lifetimes. `mark()` on entry,
  `freeToMarker()` on exit.
- **Pool** — many objects of one size that come and go individually
  (components, particles, list nodes).

All three return **raw, uninitialized storage** and run no
constructors/destructors — place objects with placement-new and destroy them
yourself (or keep them trivially destructible). Each can either own a heap buffer
or borrow an external one.

```cpp
#include <zukiru/memory/memory.hpp>
using namespace zukiru::memory;

ArenaAllocator frame(1 << 20);            // 1 MiB scratch
auto* verts = frame.allocate<Vertex>(1024);
// ... use for this frame ...
frame.reset();                             // reclaim all at once

PoolAllocator particles(sizeof(Particle), 10'000);
void* p = particles.allocate();
particles.free(p);
```

## Handles

`Handle<Tag>` packs a 32-bit index and 32-bit generation into 8 bytes. The
phantom `Tag` keeps `Handle<MeshTag>` and `Handle<TextureTag>` from being mixed
up. A container that bumps a slot's generation on reuse can then reject a stale
handle — safe weak references without raw pointers. (The `containers` module's
slot map builds on this.)

## Tests

```bash
ctest --preset debug -R '^memory\.'
```

Also run under Address/UB sanitizers via `--preset asan`.

## Dependencies

`core` (Layer 0). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).

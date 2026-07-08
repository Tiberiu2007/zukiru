# jobs

**Layer 1 — services.** A thread-pool job system: fire-and-forget tasks, futures,
and a blocking parallel-for. Public API depends only on [`core`](../core); the
implementation uses [`platform`](../platform) for hardware-thread detection and
worker naming. Namespace `zuki::jobs`.

## API

```cpp
#include <zuki/jobs/jobs.hpp>
using namespace zuki::jobs;

JobSystem jobs;                       // one worker per hardware thread (or JobSystem{n})

jobs.submit([]{ bakeLightmap(); });   // fire-and-forget

auto future = jobs.async([]{ return loadMesh(); });   // result via std::future
Mesh m = future.get();

jobs.parallelFor(particles.size(), [&](usize begin, usize end) {
    for (usize i = begin; i < end; ++i) particles[i].update(dt);
});

jobs.waitIdle();                      // block until all queued work is done
```

| Method | Behavior |
|--------|----------|
| `submit(task)` | Enqueue a `void()` task; returns immediately. |
| `async(fn) -> future<R>` | Enqueue and get a future for the result. |
| `parallelFor(count, chunkSize, body)` | Split `[0,count)` into chunks, run `body(begin,end)` across the pool, block until done. |
| `parallelFor(count, body)` | As above with an auto-chosen chunk size (~4 chunks/worker). |
| `waitIdle()` | Block until the queue is empty and nothing is executing. |

## Design

A single shared task queue guarded by a mutex + condition variable feeds N
worker threads. The distinguishing detail is **help-while-waiting**: a thread
blocked in `parallelFor` or `waitIdle` runs pending tasks itself rather than just
sleeping. That means:

- no worker sits idle while there is queued work, and
- nested dispatch (calling `parallelFor` from inside a job) can't deadlock the
  pool by consuming all workers.

`parallelFor` tracks its own batch completion with an atomic counter, so it only
waits for *its* chunks, not unrelated work.

This is a straightforward mutex-based pool, deliberately simple and correct. A
lock-free / work-stealing deque is a possible future optimization if profiling
shows queue contention.

## Verification

Tested under all four presets: `debug`, `release`, `asan`, and — importantly for
concurrency — `tsan` (no data races reported).

```bash
ctest --preset debug -R '^jobs\.'
ctest --preset tsan  -R '^jobs\.'
```

## Dependencies

`core` (public), `platform` (private). Layer 1. See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).

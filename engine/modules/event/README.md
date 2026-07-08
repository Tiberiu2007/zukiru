# event

**Layer 1 — services.** A type-safe publish/subscribe event bus with both
synchronous and deferred dispatch. Header-only. Depends only on
[`core`](../core). Namespace `zuki::event`.

## Concepts

- **Event** — any plain user type. No base class, no registration.
- **Handler** — a `void(const E&)` callable subscribed to event type `E`.
- **Subscription** — an RAII token returned by `subscribe`. The handler is
  removed when the token is destroyed (unless `detach()`ed). `subscribe` is
  `[[nodiscard]]` so you can't accidentally drop the token and silently
  unsubscribe.

## Usage

```cpp
#include <zuki/event/event.hpp>
using namespace zuki::event;

struct DamageTaken { EntityId who; f32 amount; };

EventBus bus;
auto sub = bus.subscribe<DamageTaken>([](const DamageTaken& e) {
    log(e.who, e.amount);
});

bus.publish(DamageTaken{hero, 10.0f});   // synchronous: handlers run now

bus.enqueue(DamageTaken{hero, 5.0f});    // deferred...
bus.dispatchQueued();                    // ...delivered here, in order
```

Keep the `Subscription` alive as long as you want to receive events. To
subscribe for the life of the bus, call `sub.detach()`.

## Synchronous vs queued

- **`publish(e)`** dispatches immediately to all current handlers.
- **`enqueue(e)` + `dispatchQueued()`** defers delivery to a chosen point (e.g.
  once per frame), decoupling producers from when handlers run. Events enqueued
  *during* dispatch are held for the next flush, so it can't loop forever.

## Semantics & threading

- Dispatch uses a **snapshot** of the handler list, so subscribing or
  unsubscribing from inside a handler is safe — it affects future dispatches,
  not the one in progress.
- An `EventBus` is **not synchronized**: use one per thread, or add external
  locking. A `Subscription` must not outlive its bus.
- Type identity is RTTI-free (address of a per-type static), so this works with
  `-fno-rtti`.

## Tests

```bash
ctest --preset debug -R '^event\.'
```

Also exercised under `--preset asan`.

## Dependencies

`core` (Layer 1). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).

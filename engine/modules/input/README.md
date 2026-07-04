# input

**Layer 1 — services.** Device abstraction and action mapping. Two pieces:

- **`InputState`** — the per-frame snapshot of keyboard, mouse and gamepad, with
  one-frame *edge* detection (`pressed` / `released`, not just `down`).
- **`ActionMap`** — rebindable, named actions (`"Jump"`, `"MoveX"`) layered over
  physical inputs, so gameplay code never hard-codes a key.

Header-only, depends only on [`core`](../core). Namespace `zukiru::input`.

## A sink, not a poller

`InputState` does not talk to the OS. A platform backend (or a test) **pushes**
raw device events into it; game code queries it. This keeps input decoupled from
the windowing backend, which is deferred — see
[`docs/adr/0003-windowing-backend.md`](../../../docs/adr/0003-windowing-backend.md).
When a backend lands it simply translates native codes into `Key`/`MouseButton`/…
and calls the `on*()` methods.

## Per-frame flow

```cpp
#include <zukiru/input/input.hpp>
using namespace zukiru::input;

InputState state;

// each frame:
state.beginFrame();                 // roll current -> previous, clear deltas
/* pump OS events: */
state.onKey(Key::Space, true);      // key down
state.onMouseMove(x, y);
state.onScroll(0.0f, dy);

if (state.keyPressed(Key::Space)) jump();   // true only on the down edge
Vec2 look = state.mouseDelta();             // movement since frame start
```

`beginFrame()` must be called exactly once per frame **before** events are
pumped: it makes the current state the baseline for edge detection and resets the
per-frame accumulators (mouse delta, scroll). `reset()` clears everything — call
it on focus loss to avoid stuck keys.

## Action mapping

```cpp
ActionMap map;
map.bindAction("Jump", ButtonBinding::key(Key::Space));
map.bindAction("Jump", ButtonBinding::pad(GamepadButton::A));      // extra binding
map.bindAction("Save", ButtonBinding::key(Key::S).withMods(KeyMods::Control));

map.bindAxis("MoveX", AxisBinding::keys(Key::D, Key::A));          // +D / -A
map.bindAxis("MoveX", AxisBinding::gamepad(GamepadAxis::LeftX, 1.0f, 0.15f));

if (map.actionPressed("Jump", state)) jump();
f32 x = map.axisValue("MoveX", state);   // [-1, 1]
```

- A **button action** fires if *any* of its bindings fires — that's how one action
  serves keyboard and gamepad at once.
- An **axis action** sums its bindings and clamps to `[-1, 1]`. Axis sources are
  either an analog gamepad axis (with `scale` — pass `-1` to invert — and a
  `deadzone`) or a digital key/button pair (`+1` / `-1`).
- Bindings may require keyboard modifiers via `withMods()` (e.g. Ctrl+S).

## Scope

MVP: single gamepad (index 0), no text/IME input, no per-binding rebinding UI or
serialization yet — those layer on without breaking this API. The `Key` set is
engine-defined (US-layout physical meaning); a backend maps native scancodes onto
it.

## Tests

```bash
ctest --preset debug -R '^input\.'
```

Also exercised under `--preset asan`.

## Dependencies

`core` (Layer 1). See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).

# app

**Layer 4 (needed early) — the engine host.** `App` owns the window, GPU device
and input state, wires them together, and runs the main loop. This is what turns
the engine's modules into a **runnable program**: the first window that opens,
clears, and responds to input.

Depends on [`core`](../core), [`platform`](../platform), [`input`](../input) and
[`render`](../render). Namespace `zukiru::app`.

## Writing a game

Subclass `Application` and override the hooks you need — all default to no-ops:

```cpp
#include <zukiru/app/app.hpp>
using namespace zukiru;

struct MyGame : app::Application {
    void onStart(app::App& a) override {
        a.device().setClearColor({0.1f, 0.1f, 0.15f, 1.0f});
    }
    void onUpdate(app::App& a, f32 dt) override {
        if (a.input().keyPressed(input::Key::Escape)) a.requestQuit();
    }
    void onRender(app::App& a) override {
        a.device().bindPipeline(pipeline);
        a.device().bindVertexBuffer(mesh);
        a.device().draw(3);
    }
};

int main() {
    auto app = app::App::create({.window = {.title = "My Game"}});
    if (app.isErr()) return 1;
    MyGame game;
    app.value()->run(game);   // blocks until the window closes / requestQuit()
    return 0;
}
```

## Lifecycle

`App::create(config)` initializes subsystems **in order** — window, then GPU
device (surface from the window, clear color from the config) — and returns an
`Error` if a display or GPU is missing. `run(game)` then, each frame:

1. computes `deltaTime()` (monotonic clock),
2. rolls input to a new frame, pumps OS events, and translates them into the
   `InputState` (via the input↔platform bridge),
3. dispatches `onEvent` per event (handling `WindowResize` → swapchain resize),
4. calls `onUpdate(app, dt)`,
5. clears the frame, calls `onRender(app)` (record draws via `app.device()` inside
   the render pass), then presents (recreating the swapchain if it went stale).

The loop runs until the window closes or `requestQuit()` is called; then the GPU
is drained and `onShutdown` fires. Teardown (device before window) is automatic.

`App` exposes `window()`, `device()`, `input()`, `deltaTime()` and `frameCount()`
to the hooks.

## Scope

The loop is a single variable-timestep update + clear/present. Once the render
module grows a pipeline/shader path (Milestone 4 shader toolchain), the same loop
draws real geometry with no structural change here. Deferred (additive): a fixed
timestep / accumulator for simulation, subsystem registration beyond the built-in
three, and multi-window support.

## Tests

```bash
ctest --preset debug -R '^app\.'      # config defaults (CI-safe)
zukiru_app_tests "[.app]"             # real GPU: run the loop, quit after N frames
```

The hidden `[.app]` test opens a real window, runs the loop on the actual device,
verifies the lifecycle hooks fire and the frame counter advances, then quits —
verified on an NVIDIA RTX 3060 and clean under ASan.

## Dependencies

`core`, `platform`, `input`, `render`. See the dependency table in
[`agents/PROJECT_STRUCTURE.md`](../../../agents/PROJECT_STRUCTURE.md).

// Application — the hooks a game implements to run inside the engine host.
//
// Subclass and override what you need (all default to no-ops), then hand an
// instance to App::run(). The App& gives you the window, GPU device, and input
// state each callback.
//
//   struct MyGame : app::Application {
//       void onStart(app::App& a) override { a.device().setClearColor({...}); }
//       void onUpdate(app::App& a, f32 dt) override {
//           if (a.input().keyPressed(input::Key::Escape)) a.requestQuit();
//       }
//   };
#pragma once

#include <zukiru/core/types.hpp>
#include <zukiru/platform/window.hpp>

namespace zukiru::app {

class App;

class Application {
public:
    Application() = default;
    virtual ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    // Once, before the first frame.
    virtual void onStart(App& app) { (void)app; }

    // Once per frame, with the elapsed seconds since the previous frame.
    virtual void onUpdate(App& app, f32 dt) {
        (void)app;
        (void)dt;
    }

    // For each window/input event this frame (already applied to input state).
    virtual void onEvent(App& app, const platform::WindowEvent& event) {
        (void)app;
        (void)event;
    }

    // Once per frame, inside the render pass (between beginFrame and endFrame) —
    // record draw commands here via app.device(). Skipped on frames where the
    // swapchain had to be recreated.
    virtual void onRender(App& app) { (void)app; }

    // Once, after the loop exits and the GPU is idle.
    virtual void onShutdown(App& app) { (void)app; }
};

}  // namespace zukiru::app

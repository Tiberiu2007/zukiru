// App — the engine host: owns the window, GPU device and input state, wires them
// together, and drives the main loop.
//
//   auto app = app::App::create({.window = {.title = "Zukiru"}});
//   if (app.isErr()) return fail(app.error());
//   MyGame game;
//   app.value()->run(game);   // blocks until the window closes / requestQuit()
//
// create() initializes subsystems in order (window → device); run() pumps events
// into the input state, clears/presents each frame, and dispatches the game hooks.
// Shutdown (GPU idle, teardown in reverse) is automatic.
#pragma once

#include <zukiru/app/application.hpp>
#include <zukiru/core/result.hpp>
#include <zukiru/core/types.hpp>
#include <zukiru/input/input_state.hpp>
#include <zukiru/platform/window.hpp>
#include <zukiru/render/rhi.hpp>

#include <memory>

namespace zukiru::app {

struct AppConfig {
    platform::WindowConfig window{};
    render::DeviceConfig device{};
    render::Color clearColor{0.05f, 0.05f, 0.08f, 1.0f};
};

class App {
public:
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;

    // Bring up the window and GPU device. Errors if a display or GPU is missing.
    [[nodiscard]] static Result<std::unique_ptr<App>> create(const AppConfig& config = {});

    // Run the main loop until the window closes or requestQuit() is called.
    void run(Application& game);

    // Ask the loop to exit after the current frame.
    void requestQuit() noexcept { quit_ = true; }

    [[nodiscard]] platform::Window& window() noexcept { return *window_; }
    [[nodiscard]] render::Device& device() noexcept { return *device_; }
    [[nodiscard]] input::InputState& input() noexcept { return input_; }

    // Seconds elapsed during the previous frame.
    [[nodiscard]] f32 deltaTime() const noexcept { return deltaTime_; }
    // Number of frames run so far.
    [[nodiscard]] u64 frameCount() const noexcept { return frameCount_; }

private:
    App(std::unique_ptr<platform::Window> window, std::unique_ptr<render::Device> device) noexcept;

    std::unique_ptr<platform::Window> window_;
    std::unique_ptr<render::Device> device_;
    input::InputState input_;
    f32 deltaTime_ = 0.0f;
    u64 frameCount_ = 0;
    bool quit_ = false;
};

}  // namespace zukiru::app

#include <zuki/app/app.hpp>

#include <zuki/core/time.hpp>
#include <zuki/input/platform_bridge.hpp>

#include <utility>

namespace zuki::app {

App::App(std::unique_ptr<platform::Window> window,
         std::unique_ptr<render::Device> device) noexcept
    : window_(std::move(window)), device_(std::move(device)) {}

App::~App() = default;

Result<std::unique_ptr<App>> App::create(const AppConfig& config) {
    Result<std::unique_ptr<platform::Window>> window = platform::createWindow(config.window);
    if (window.isErr()) {
        return Err(std::move(window.error()));
    }

    Result<std::unique_ptr<render::Device>> device =
        render::createDevice(*window.value(), config.device);
    if (device.isErr()) {
        return Err(std::move(device.error()));  // window is cleaned up as it unwinds
    }
    device.value()->setClearColor(config.clearColor);

    return Ok(std::unique_ptr<App>(
        new App(std::move(window.value()), std::move(device.value()))));
}

void App::run(Application& game) {
    game.onStart(*this);

    Instant previous = Clock::now();
    while (!window_->shouldClose() && !quit_) {
        const Instant now = Clock::now();
        deltaTime_ = static_cast<f32>((now - previous).seconds());
        previous = now;

        // Roll input to a new frame, pump OS events, translate them into input.
        input_.beginFrame();
        window_->pollEvents();
        input::pumpWindowEvents(input_, *window_);

        for (const platform::WindowEvent& event : window_->events()) {
            if (event.type == platform::EventType::WindowResize) {
                device_->resize(event.width, event.height);
            }
            game.onEvent(*this, event);
        }

        game.onUpdate(*this, deltaTime_);

        if (device_->beginFrame()) {
            device_->beginSwapchainPass();
            game.onRender(*this);  // record draws into the swapchain pass
            device_->endRenderPass();
            device_->endFrame();
        } else {
            // Swapchain out of date — resize to the current window and retry next frame.
            const platform::WindowExtent extent = window_->extent();
            device_->resize(extent.width, extent.height);
        }
        ++frameCount_;
    }

    device_->waitIdle();
    game.onShutdown(*this);
}

}  // namespace zuki::app

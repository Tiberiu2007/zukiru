// RHI — the render hardware interface: a backend-agnostic handle to the GPU.
//
// Game/scene code talks to this, never to Vulkan directly, so a second backend
// (D3D12/Metal) can drop in later. This first cut brings up a swapchain and
// clears it each frame; command recording, resources, and pipelines extend the
// `Device` interface without breaking callers (see ADR 0006).
//
//   auto device = render::createDevice(*window);
//   device.value()->setClearColor({0.1f, 0.1f, 0.15f, 1.0f});
//   while (!window->shouldClose()) {
//       window->pollEvents();
//       if (!device.value()->beginFrame()) { device.value()->resize(w, h); continue; }
//       device.value()->endFrame();   // submit + present
//   }
#pragma once

#include <zukiru/core/result.hpp>
#include <zukiru/core/types.hpp>

#include <memory>
#include <string_view>

namespace zukiru::platform {
class Window;
}

namespace zukiru::render {

enum class Backend : u8 {
    Vulkan,
};

// Linear RGBA in [0, 1].
struct Color {
    f32 r = 0.0f;
    f32 g = 0.0f;
    f32 b = 0.0f;
    f32 a = 1.0f;

    friend constexpr bool operator==(Color, Color) = default;
};

struct DeviceConfig {
    Backend backend = Backend::Vulkan;
    bool enableValidation = false;  // Vulkan validation layers (dev builds)
    bool vsync = true;              // FIFO present mode when true
};

// A GPU device bound to a window's swapchain.
class Device {
public:
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    virtual ~Device() = default;

    // Acquire the next swapchain image and begin recording. Returns false if the
    // swapchain is out of date (e.g. the window resized) — call resize() with the
    // new size and retry next frame.
    [[nodiscard]] virtual bool beginFrame() = 0;

    // Finish recording, submit, and present the acquired image.
    virtual void endFrame() = 0;

    // The color each acquired frame is cleared to. Applies from the next frame.
    virtual void setClearColor(Color color) = 0;

    // Recreate the swapchain for a new framebuffer size (call on window resize).
    virtual void resize(u32 width, u32 height) = 0;

    // Block until the GPU has finished all submitted work (e.g. before teardown).
    virtual void waitIdle() = 0;

    [[nodiscard]] virtual Backend backend() const = 0;
    [[nodiscard]] virtual std::string_view deviceName() const = 0;

protected:
    Device() = default;
};

// Create a device rendering into `window`. Errors if the backend can't be
// initialized (no compatible GPU, missing extensions, surface failure, ...).
[[nodiscard]] Result<std::unique_ptr<Device>> createDevice(const platform::Window& window,
                                                           const DeviceConfig& config = {});

}  // namespace zukiru::render

#include <zukiru/render/rhi.hpp>

#include <zukiru/platform/window.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace zukiru;

// End-to-end Vulkan bring-up: needs a real GPU + display, so it is hidden by
// default (Catch2 runs '.'-tagged tests only on request). Run it via:
//   zukiru_render_tests "[.gpu]"
TEST_CASE("Vulkan device clears and presents frames", "[.gpu]") {
    Result<std::unique_ptr<platform::Window>> windowResult =
        platform::createWindow({.title = "Zukiru render test", .width = 640, .height = 480});
    REQUIRE(windowResult.isOk());
    std::unique_ptr<platform::Window>& window = windowResult.value();

    Result<std::unique_ptr<render::Device>> deviceResult = render::createDevice(*window);
    if (deviceResult.isErr()) {
        FAIL(deviceResult.error().message);
    }
    std::unique_ptr<render::Device>& device = deviceResult.value();

    REQUIRE(device->backend() == render::Backend::Vulkan);
    REQUIRE_FALSE(device->deviceName().empty());
    INFO("GPU: " << device->deviceName());

    device->setClearColor({0.1f, 0.2f, 0.4f, 1.0f});

    // Render several frames; a resize partway through exercises swapchain
    // recreation.
    for (int frame = 0; frame < 20; ++frame) {
        window->pollEvents();
        if (frame == 10) {
            device->resize(800, 600);
        }
        if (!device->beginFrame()) {
            device->resize(window->extent().width, window->extent().height);
            continue;
        }
        device->endFrame();
    }

    device->waitIdle();
    SUCCEED("rendered 20 frames without error");
}

#include <zukiru/render/rhi.hpp>

#include <zukiru/platform/window.hpp>

#include "mesh_shaders.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>

using namespace zukiru;

namespace {

struct Vertex {
    f32 position[2];
    f32 color[3];
};

// A user-supplied RGB triangle in a vertex buffer (no built-in geometry).
constexpr Vertex kTriangle[3] = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
};

}  // namespace

// Needs a real GPU + display, so hidden by default. Run via:
//   zukiru_render_tests "[.gpu]"
TEST_CASE("Vulkan device draws user geometry", "[.gpu]") {
    Result<std::unique_ptr<platform::Window>> windowResult =
        platform::createWindow({.title = "Zukiru render test", .width = 640, .height = 480});
    REQUIRE(windowResult.isOk());
    std::unique_ptr<platform::Window>& window = windowResult.value();

    Result<std::unique_ptr<render::Device>> deviceResult = render::createDevice(*window);
    if (deviceResult.isErr()) {
        FAIL(deviceResult.error().message);
    }
    std::unique_ptr<render::Device>& device = deviceResult.value();
    INFO("GPU: " << device->deviceName());

    // Upload the triangle and build a pipeline that reads it.
    const render::BufferHandle vbo =
        device->createBuffer(render::BufferKind::Vertex, kTriangle, sizeof(kTriangle));
    REQUIRE(vbo.valid());

    render::PipelineDesc desc;
    desc.vertexSpirv = render::kMeshVertSpirv;
    desc.fragmentSpirv = render::kMeshFragSpirv;
    desc.vertexLayout.stride = sizeof(Vertex);
    desc.vertexLayout.attributes = {
        {.location = 0, .format = render::VertexFormat::Float32x2, .offset = 0},
        {.location = 1, .format = render::VertexFormat::Float32x3, .offset = sizeof(f32) * 2},
    };
    Result<render::PipelineHandle> pipeline = device->createPipeline(desc);
    if (pipeline.isErr()) {
        FAIL(pipeline.error().message);
    }
    REQUIRE(pipeline.value().valid());

    device->setClearColor({0.1f, 0.1f, 0.15f, 1.0f});

    for (int frame = 0; frame < 20; ++frame) {
        window->pollEvents();
        if (frame == 10) {
            device->resize(800, 600);  // exercise swapchain recreation mid-run
        }
        if (!device->beginFrame()) {
            device->resize(window->extent().width, window->extent().height);
            continue;
        }
        device->bindPipeline(pipeline.value());
        device->bindVertexBuffer(vbo);
        device->draw(3);
        device->endFrame();
    }

    device->waitIdle();
    device->destroyPipeline(pipeline.value());
    device->destroyBuffer(vbo);
    SUCCEED("drew user geometry for 20 frames");
}

TEST_CASE("invalid SPIR-V is rejected by createPipeline", "[.gpu]") {
    Result<std::unique_ptr<platform::Window>> windowResult =
        platform::createWindow({.title = "Zukiru render test", .width = 320, .height = 240});
    REQUIRE(windowResult.isOk());
    Result<std::unique_ptr<render::Device>> deviceResult =
        render::createDevice(*windowResult.value());
    REQUIRE(deviceResult.isOk());

    static constexpr u32 kGarbage[] = {0xdeadbeefu, 0x12345678u, 0x00000000u};
    render::PipelineDesc desc;
    desc.vertexSpirv = kGarbage;
    desc.fragmentSpirv = kGarbage;
    REQUIRE(deviceResult.value()->createPipeline(desc).isErr());
}

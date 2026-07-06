#include <zukiru/render/rhi.hpp>

#include <zukiru/math/math.hpp>
#include <zukiru/platform/window.hpp>
#include <zukiru/render/camera.hpp>
#include <zukiru/render/primitives.hpp>

#include "cube_shaders.hpp"
#include "mesh_shaders.hpp"
#include "tex_shaders.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
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

namespace {

struct TexVertex {
    f32 position[2];
    f32 uv[2];
};

constexpr TexVertex kTexTriangle[3] = {
    {{0.0f, -0.5f}, {0.5f, 0.0f}},
    {{0.5f, 0.5f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f}, {0.0f, 1.0f}},
};

// A 2x2 RGBA checkerboard.
constexpr std::uint8_t kCheckerPixels[2 * 2 * 4] = {
    255, 255, 255, 255, 40, 40, 40, 255,   // row 0: white, dark
    40,  40,  40,  255, 255, 255, 255, 255,  // row 1: dark, white
};

}  // namespace

TEST_CASE("Vulkan device draws a textured, uniform-transformed triangle", "[.gpu]") {
    Result<std::unique_ptr<platform::Window>> windowResult =
        platform::createWindow({.title = "Zukiru texture test", .width = 640, .height = 480});
    REQUIRE(windowResult.isOk());
    std::unique_ptr<platform::Window>& window = windowResult.value();

    Result<std::unique_ptr<render::Device>> deviceResult = render::createDevice(*window);
    if (deviceResult.isErr()) {
        FAIL(deviceResult.error().message);
    }
    std::unique_ptr<render::Device>& device = deviceResult.value();

    // Geometry, a uniform transform (the camera's view-projection), and a texture.
    const render::BufferHandle vbo =
        device->createBuffer(render::BufferKind::Vertex, kTexTriangle, sizeof(kTexTriangle));
    REQUIRE(vbo.valid());

    const math::Mat4 mvp = math::Mat4::identity();
    const render::BufferHandle ubo =
        device->createBuffer(render::BufferKind::Uniform, mvp.e, sizeof(mvp.e));
    REQUIRE(ubo.valid());

    const render::TextureHandle texture = device->createTexture(2, 2, kCheckerPixels);
    REQUIRE(texture.valid());

    render::PipelineDesc desc;
    desc.vertexSpirv = render::kTexVertSpirv;
    desc.fragmentSpirv = render::kTexFragSpirv;
    desc.vertexLayout.stride = sizeof(TexVertex);
    desc.vertexLayout.attributes = {
        {.location = 0, .format = render::VertexFormat::Float32x2, .offset = 0},
        {.location = 1, .format = render::VertexFormat::Float32x2, .offset = sizeof(f32) * 2},
    };
    desc.bindings = {render::BindingType::UniformBuffer, render::BindingType::Texture};
    Result<render::PipelineHandle> pipeline = device->createPipeline(desc);
    if (pipeline.isErr()) {
        FAIL(pipeline.error().message);
    }

    const render::BindGroupEntry entries[] = {
        {.binding = 0, .buffer = ubo, .texture = {}},
        {.binding = 1, .buffer = {}, .texture = texture},
    };
    Result<render::BindGroupHandle> bindGroup =
        device->createBindGroup(pipeline.value(), entries);
    if (bindGroup.isErr()) {
        FAIL(bindGroup.error().message);
    }

    device->setClearColor({0.05f, 0.05f, 0.08f, 1.0f});
    for (int frame = 0; frame < 20; ++frame) {
        window->pollEvents();
        if (!device->beginFrame()) {
            device->resize(window->extent().width, window->extent().height);
            continue;
        }
        device->bindPipeline(pipeline.value());
        device->bindBindGroup(bindGroup.value());
        device->bindVertexBuffer(vbo);
        device->draw(3);
        device->endFrame();
    }

    device->waitIdle();
    device->destroyBindGroup(bindGroup.value());
    device->destroyPipeline(pipeline.value());
    device->destroyTexture(texture);
    device->destroyBuffer(ubo);
    device->destroyBuffer(vbo);
    SUCCEED("drew a textured uniform triangle for 20 frames");
}

namespace {

// Matches the cube shader's uniform block { mat4 mvp; mat4 model; } (std140).
struct CubeUniforms {
    math::Mat4 mvp;
    math::Mat4 model;
};

}  // namespace

// A rotating, indexed, textured 3D cube — exercises depth buffering (the far
// faces must be occluded by the near ones) plus indexed draws and a perspective
// camera whose matrix is re-uploaded every frame.
TEST_CASE("Vulkan device draws a depth-tested rotating textured cube", "[.gpu]") {
    Result<std::unique_ptr<platform::Window>> windowResult =
        platform::createWindow({.title = "Zukiru cube test", .width = 800, .height = 600});
    REQUIRE(windowResult.isOk());
    std::unique_ptr<platform::Window>& window = windowResult.value();

    Result<std::unique_ptr<render::Device>> deviceResult = render::createDevice(*window);
    if (deviceResult.isErr()) {
        FAIL(deviceResult.error().message);
    }
    std::unique_ptr<render::Device>& device = deviceResult.value();
    INFO("GPU: " << device->deviceName());

    // Geometry: a unit cube as vertex + index buffers.
    const render::MeshData cube = render::cubeMesh();
    const render::BufferHandle vbo = device->createBuffer(
        render::BufferKind::Vertex, cube.vertices.data(), cube.vertexBytes());
    const render::BufferHandle ibo =
        device->createBuffer(render::BufferKind::Index, cube.indices.data(), cube.indexBytes());
    REQUIRE(vbo.valid());
    REQUIRE(ibo.valid());

    // A per-frame uniform (mvp + model) and a texture to sample.
    CubeUniforms uniforms{math::Mat4::identity(), math::Mat4::identity()};
    const render::BufferHandle ubo =
        device->createBuffer(render::BufferKind::Uniform, &uniforms, sizeof(uniforms));
    REQUIRE(ubo.valid());
    const render::TextureHandle texture = device->createTexture(2, 2, kCheckerPixels);
    REQUIRE(texture.valid());

    render::PipelineDesc desc;
    desc.vertexSpirv = render::kCubeVertSpirv;
    desc.fragmentSpirv = render::kCubeFragSpirv;
    desc.vertexLayout.stride = sizeof(render::MeshVertex);
    desc.vertexLayout.attributes = {
        {.location = 0, .format = render::VertexFormat::Float32x3, .offset = 0},
        {.location = 1, .format = render::VertexFormat::Float32x3, .offset = sizeof(f32) * 3},
        {.location = 2, .format = render::VertexFormat::Float32x2, .offset = sizeof(f32) * 6},
    };
    desc.bindings = {render::BindingType::UniformBuffer, render::BindingType::Texture};
    // depthTest / depthWrite default to true — opaque 3D geometry.
    Result<render::PipelineHandle> pipeline = device->createPipeline(desc);
    if (pipeline.isErr()) {
        FAIL(pipeline.error().message);
    }

    const render::BindGroupEntry entries[] = {
        {.binding = 0, .buffer = ubo, .texture = {}},
        {.binding = 1, .buffer = {}, .texture = texture},
    };
    Result<render::BindGroupHandle> bindGroup = device->createBindGroup(pipeline.value(), entries);
    if (bindGroup.isErr()) {
        FAIL(bindGroup.error().message);
    }

    render::Camera camera;
    const platform::WindowExtent extent = window->extent();
    const f32 aspect = static_cast<f32>(extent.width) / static_cast<f32>(extent.height);
    camera.setPerspective(math::radians(60.0f), aspect, 0.1f, 100.0f);
    camera.lookAt({2.5f, 2.0f, 3.0f}, {0.0f, 0.0f, 0.0f}, math::Vec3::unitY());

    device->setClearColor({0.05f, 0.06f, 0.1f, 1.0f});
    for (int frame = 0; frame < 30; ++frame) {
        window->pollEvents();

        // Spin the cube; re-upload mvp = viewProjection * model each frame.
        const f32 angle = static_cast<f32>(frame) * 0.1f;
        const math::Mat4 model =
            math::toMat4(math::Quat::fromEuler(angle * 0.5f, angle, 0.0f));
        uniforms.model = model;
        uniforms.mvp = camera.viewProjection() * model;
        device->updateBuffer(ubo, &uniforms, sizeof(uniforms));

        if (!device->beginFrame()) {
            device->resize(window->extent().width, window->extent().height);
            continue;
        }
        device->bindPipeline(pipeline.value());
        device->bindBindGroup(bindGroup.value());
        device->bindVertexBuffer(vbo);
        device->bindIndexBuffer(ibo, render::IndexType::U16);
        device->drawIndexed(static_cast<u32>(cube.indices.size()));
        device->endFrame();
    }

    device->waitIdle();
    device->destroyBindGroup(bindGroup.value());
    device->destroyPipeline(pipeline.value());
    device->destroyTexture(texture);
    device->destroyBuffer(ubo);
    device->destroyBuffer(ibo);
    device->destroyBuffer(vbo);
    SUCCEED("drew a depth-tested rotating textured cube for 30 frames");
}

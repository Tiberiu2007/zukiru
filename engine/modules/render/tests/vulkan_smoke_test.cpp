#include <zukiru/render/rhi.hpp>

#include <zukiru/math/math.hpp>
#include <zukiru/platform/window.hpp>
#include <zukiru/render/camera.hpp>
#include <zukiru/render/material.hpp>
#include <zukiru/render/primitives.hpp>
#include <zukiru/render/render_graph.hpp>

#include "cube_shaders.hpp"
#include "fullscreen_shaders.hpp"
#include "mesh_shaders.hpp"
#include "scene_shaders.hpp"
#include "tex_shaders.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
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
        device->beginSwapchainPass();
        device->bindPipeline(pipeline.value());
        device->bindVertexBuffer(vbo);
        device->draw(3);
        device->endRenderPass();
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
        device->beginSwapchainPass();
        device->bindPipeline(pipeline.value());
        device->bindBindGroup(bindGroup.value());
        device->bindVertexBuffer(vbo);
        device->draw(3);
        device->endRenderPass();
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
        device->beginSwapchainPass();
        device->bindPipeline(pipeline.value());
        device->bindBindGroup(bindGroup.value());
        device->bindVertexBuffer(vbo);
        device->bindIndexBuffer(ibo, render::IndexType::U16);
        device->drawIndexed(static_cast<u32>(cube.indices.size()));
        device->endRenderPass();
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

// The cube again, but driven by a Material (which owns the uniform buffer + bind
// group) recorded inside a RenderGraph pass — exercises both new layers end to end.
TEST_CASE("Vulkan device draws a material through a render graph", "[.gpu]") {
    Result<std::unique_ptr<platform::Window>> windowResult =
        platform::createWindow({.title = "Zukiru material test", .width = 800, .height = 600});
    REQUIRE(windowResult.isOk());
    std::unique_ptr<platform::Window>& window = windowResult.value();

    Result<std::unique_ptr<render::Device>> deviceResult = render::createDevice(*window);
    if (deviceResult.isErr()) {
        FAIL(deviceResult.error().message);
    }
    std::unique_ptr<render::Device>& device = deviceResult.value();
    INFO("GPU: " << device->deviceName());

    const render::MeshData cube = render::cubeMesh();
    const render::BufferHandle vbo = device->createBuffer(
        render::BufferKind::Vertex, cube.vertices.data(), cube.vertexBytes());
    const render::BufferHandle ibo =
        device->createBuffer(render::BufferKind::Index, cube.indices.data(), cube.indexBytes());
    const render::TextureHandle texture = device->createTexture(2, 2, kCheckerPixels);
    REQUIRE(vbo.valid());
    REQUIRE(ibo.valid());
    REQUIRE(texture.valid());

    // A material matching the cube shaders: uniform block { mat4 mvp; mat4 model; }
    // at binding 0, sampler at binding 1.
    render::MaterialLayout layout;
    layout.addMat4("mvp").addMat4("model").addTexture("tex");
    REQUIRE(layout.uniformSize() == 128);

    render::MaterialTemplateDesc templateDesc;
    templateDesc.layout = layout;
    templateDesc.vertexSpirv = render::kCubeVertSpirv;
    templateDesc.fragmentSpirv = render::kCubeFragSpirv;
    templateDesc.vertexLayout.stride = sizeof(render::MeshVertex);
    templateDesc.vertexLayout.attributes = {
        {.location = 0, .format = render::VertexFormat::Float32x3, .offset = 0},
        {.location = 1, .format = render::VertexFormat::Float32x3, .offset = sizeof(f32) * 3},
        {.location = 2, .format = render::VertexFormat::Float32x2, .offset = sizeof(f32) * 6},
    };
    Result<std::unique_ptr<render::MaterialTemplate>> materialTemplate =
        render::MaterialTemplate::create(*device, templateDesc);
    if (materialTemplate.isErr()) {
        FAIL(materialTemplate.error().message);
    }
    std::unique_ptr<render::Material> material = materialTemplate.value()->instantiate();
    material->setTexture("tex", texture);

    render::Camera camera;
    const platform::WindowExtent extent = window->extent();
    const f32 aspect = static_cast<f32>(extent.width) / static_cast<f32>(extent.height);
    camera.setPerspective(math::radians(60.0f), aspect, 0.1f, 100.0f);
    camera.lookAt({2.5f, 2.0f, 3.0f}, {0.0f, 0.0f, 0.0f}, math::Vec3::unitY());

    // One graph, reused each frame: a single pass that draws the material.
    render::RenderGraph graph;
    const render::RgResource backbuffer = graph.importResource("backbuffer");
    graph.addPass("cube").writes(backbuffer).setExecute([&](render::PassContext& ctx) {
        material->bind(ctx.device);
        ctx.device.bindVertexBuffer(vbo);
        ctx.device.bindIndexBuffer(ibo, render::IndexType::U16);
        ctx.device.drawIndexed(static_cast<u32>(cube.indices.size()));
    });
    Result<render::CompiledGraph> compiled = graph.compile();
    REQUIRE(compiled.isOk());

    device->setClearColor({0.05f, 0.06f, 0.1f, 1.0f});
    for (int frame = 0; frame < 30; ++frame) {
        window->pollEvents();

        const f32 angle = static_cast<f32>(frame) * 0.1f;
        const math::Mat4 model = math::toMat4(math::Quat::fromEuler(angle * 0.5f, angle, 0.0f));
        material->setMat4("mvp", camera.viewProjection() * model).setMat4("model", model);

        if (!device->beginFrame()) {
            device->resize(window->extent().width, window->extent().height);
            continue;
        }
        device->beginSwapchainPass();
        graph.execute(*device, compiled.value());
        device->endRenderPass();
        device->endFrame();
    }

    device->waitIdle();
    material.reset();            // frees uniform buffer + bind group
    materialTemplate.value().reset();  // frees the pipeline
    device->destroyTexture(texture);
    device->destroyBuffer(ibo);
    device->destroyBuffer(vbo);
    SUCCEED("drew a material through a render graph for 30 frames");
}

// Two passes in one frame: render the cube into an offscreen target, then sample
// that target onto the screen with a fullscreen triangle. Exercises render targets,
// render-target-as-texture, and cross-pass synchronization.
TEST_CASE("Vulkan device renders offscreen and samples the result to screen", "[.gpu]") {
    Result<std::unique_ptr<platform::Window>> windowResult =
        platform::createWindow({.title = "Zukiru offscreen test", .width = 800, .height = 600});
    REQUIRE(windowResult.isOk());
    std::unique_ptr<platform::Window>& window = windowResult.value();

    Result<std::unique_ptr<render::Device>> deviceResult = render::createDevice(*window);
    if (deviceResult.isErr()) {
        FAIL(deviceResult.error().message);
    }
    std::unique_ptr<render::Device>& device = deviceResult.value();
    INFO("GPU: " << device->deviceName());

    // Offscreen target the cube is rendered into.
    const render::RenderTargetHandle target =
        device->createRenderTarget({.width = 512, .height = 512, .clearColor = {0.1f, 0.05f, 0.2f, 1.0f}});
    REQUIRE(target.valid());
    const render::TextureHandle targetTexture = device->renderTargetTexture(target);
    REQUIRE(targetTexture.valid());

    // Cube geometry + a uniform transform + a checker texture.
    const render::MeshData cube = render::cubeMesh();
    const render::BufferHandle vbo = device->createBuffer(
        render::BufferKind::Vertex, cube.vertices.data(), cube.vertexBytes());
    const render::BufferHandle ibo =
        device->createBuffer(render::BufferKind::Index, cube.indices.data(), cube.indexBytes());
    const render::TextureHandle checker = device->createTexture(2, 2, kCheckerPixels);

    struct CubeUbo {
        math::Mat4 mvp;
        math::Mat4 model;
    } ubo{math::Mat4::identity(), math::Mat4::identity()};
    const render::BufferHandle uboBuffer =
        device->createBuffer(render::BufferKind::Uniform, &ubo, sizeof(ubo));

    render::PipelineDesc cubeDesc;
    cubeDesc.vertexSpirv = render::kCubeVertSpirv;
    cubeDesc.fragmentSpirv = render::kCubeFragSpirv;
    cubeDesc.vertexLayout.stride = sizeof(render::MeshVertex);
    cubeDesc.vertexLayout.attributes = {
        {.location = 0, .format = render::VertexFormat::Float32x3, .offset = 0},
        {.location = 1, .format = render::VertexFormat::Float32x3, .offset = sizeof(f32) * 3},
        {.location = 2, .format = render::VertexFormat::Float32x2, .offset = sizeof(f32) * 6},
    };
    cubeDesc.bindings = {render::BindingType::UniformBuffer, render::BindingType::Texture};
    Result<render::PipelineHandle> cubePipeline = device->createPipeline(cubeDesc);
    REQUIRE(cubePipeline.isOk());
    const render::BindGroupEntry cubeEntries[] = {
        {.binding = 0, .buffer = uboBuffer, .texture = {}},
        {.binding = 1, .buffer = {}, .texture = checker},
    };
    Result<render::BindGroupHandle> cubeGroup =
        device->createBindGroup(cubePipeline.value(), cubeEntries);
    REQUIRE(cubeGroup.isOk());

    // Fullscreen pass: a vertexless triangle sampling the offscreen target's color.
    render::PipelineDesc fsDesc;
    fsDesc.vertexSpirv = render::kFullscreenVertSpirv;
    fsDesc.fragmentSpirv = render::kFullscreenFragSpirv;
    fsDesc.bindings = {render::BindingType::Texture};  // no vertex buffer, no depth
    fsDesc.depthTest = false;
    fsDesc.depthWrite = false;
    Result<render::PipelineHandle> fsPipeline = device->createPipeline(fsDesc);
    REQUIRE(fsPipeline.isOk());
    const render::BindGroupEntry fsEntries[] = {
        {.binding = 0, .buffer = {}, .texture = targetTexture},
    };
    Result<render::BindGroupHandle> fsGroup =
        device->createBindGroup(fsPipeline.value(), fsEntries);
    REQUIRE(fsGroup.isOk());

    render::Camera camera;
    camera.setPerspective(math::radians(60.0f), 1.0f, 0.1f, 100.0f);  // 1:1 offscreen target
    camera.lookAt({2.5f, 2.0f, 3.0f}, {0.0f, 0.0f, 0.0f}, math::Vec3::unitY());

    device->setClearColor({0.0f, 0.0f, 0.0f, 1.0f});
    for (int frame = 0; frame < 30; ++frame) {
        window->pollEvents();

        const f32 angle = static_cast<f32>(frame) * 0.1f;
        const math::Mat4 model = math::toMat4(math::Quat::fromEuler(angle * 0.5f, angle, 0.0f));
        ubo.model = model;
        ubo.mvp = camera.viewProjection() * model;
        device->updateBuffer(uboBuffer, &ubo, sizeof(ubo));

        if (!device->beginFrame()) {
            device->resize(window->extent().width, window->extent().height);
            continue;
        }
        // Pass 1: draw the cube into the offscreen target.
        device->beginRenderPass(target);
        device->bindPipeline(cubePipeline.value());
        device->bindBindGroup(cubeGroup.value());
        device->bindVertexBuffer(vbo);
        device->bindIndexBuffer(ibo, render::IndexType::U16);
        device->drawIndexed(static_cast<u32>(cube.indices.size()));
        device->endRenderPass();
        // Pass 2: composite the target onto the screen.
        device->beginSwapchainPass();
        device->bindPipeline(fsPipeline.value());
        device->bindBindGroup(fsGroup.value());
        device->draw(3);
        device->endRenderPass();
        device->endFrame();
    }

    device->waitIdle();
    device->destroyBindGroup(fsGroup.value());
    device->destroyBindGroup(cubeGroup.value());
    device->destroyPipeline(fsPipeline.value());
    device->destroyPipeline(cubePipeline.value());
    device->destroyRenderTarget(target);  // frees its color texture too
    device->destroyTexture(checker);
    device->destroyBuffer(uboBuffer);
    device->destroyBuffer(ibo);
    device->destroyBuffer(vbo);
    SUCCEED("rendered offscreen and composited to screen for 30 frames");
}

// Many moving cubes sharing one per-frame camera uniform (ring-buffered, updated
// every frame) with a per-object model matrix pushed as a push constant. Exercises
// both new paths: the uniform is safe to rewrite each frame (2 in flight), and each
// draw gets cheap per-object data with no buffer/descriptor of its own.
TEST_CASE("Vulkan device draws many cubes via a per-frame uniform + push constants", "[.gpu]") {
    Result<std::unique_ptr<platform::Window>> windowResult =
        platform::createWindow({.title = "Zukiru scene test", .width = 800, .height = 600});
    REQUIRE(windowResult.isOk());
    std::unique_ptr<platform::Window>& window = windowResult.value();

    Result<std::unique_ptr<render::Device>> deviceResult = render::createDevice(*window);
    if (deviceResult.isErr()) {
        FAIL(deviceResult.error().message);
    }
    std::unique_ptr<render::Device>& device = deviceResult.value();
    INFO("GPU: " << device->deviceName());

    const render::MeshData cube = render::cubeMesh(0.6f);
    const render::BufferHandle vbo = device->createBuffer(
        render::BufferKind::Vertex, cube.vertices.data(), cube.vertexBytes());
    const render::BufferHandle ibo =
        device->createBuffer(render::BufferKind::Index, cube.indices.data(), cube.indexBytes());
    const render::TextureHandle checker = device->createTexture(2, 2, kCheckerPixels);

    // Per-frame shared uniform: just the camera's view-projection.
    math::Mat4 viewProj = math::Mat4::identity();
    const render::BufferHandle camUbo =
        device->createBuffer(render::BufferKind::Uniform, viewProj.e, sizeof(viewProj.e));
    REQUIRE(camUbo.valid());

    render::PipelineDesc desc;
    desc.vertexSpirv = render::kSceneVertSpirv;
    desc.fragmentSpirv = render::kSceneFragSpirv;
    desc.vertexLayout.stride = sizeof(render::MeshVertex);
    desc.vertexLayout.attributes = {
        {.location = 0, .format = render::VertexFormat::Float32x3, .offset = 0},
        {.location = 1, .format = render::VertexFormat::Float32x3, .offset = sizeof(f32) * 3},
        {.location = 2, .format = render::VertexFormat::Float32x2, .offset = sizeof(f32) * 6},
    };
    desc.bindings = {render::BindingType::UniformBuffer, render::BindingType::Texture};
    desc.pushConstantSize = sizeof(math::Mat4);  // per-object model matrix
    Result<render::PipelineHandle> pipeline = device->createPipeline(desc);
    if (pipeline.isErr()) {
        FAIL(pipeline.error().message);
    }

    const render::BindGroupEntry entries[] = {
        {.binding = 0, .buffer = camUbo, .texture = {}},
        {.binding = 1, .buffer = {}, .texture = checker},
    };
    Result<render::BindGroupHandle> group = device->createBindGroup(pipeline.value(), entries);
    REQUIRE(group.isOk());

    render::Camera camera;
    const platform::WindowExtent extent = window->extent();
    camera.setPerspective(math::radians(60.0f),
                          static_cast<f32>(extent.width) / static_cast<f32>(extent.height), 0.1f,
                          100.0f);

    device->setClearColor({0.04f, 0.05f, 0.08f, 1.0f});
    for (int frame = 0; frame < 30; ++frame) {
        window->pollEvents();

        // Orbit the camera; re-upload the shared uniform each frame (ring-buffered).
        const f32 t = static_cast<f32>(frame) * 0.1f;
        camera.lookAt({std::cos(t) * 5.0f, 3.0f, std::sin(t) * 5.0f}, {0.0f, 0.0f, 0.0f},
                      math::Vec3::unitY());
        viewProj = camera.viewProjection();
        device->updateBuffer(camUbo, viewProj.e, sizeof(viewProj.e));

        if (!device->beginFrame()) {
            device->resize(window->extent().width, window->extent().height);
            continue;
        }
        device->beginSwapchainPass();
        device->bindPipeline(pipeline.value());
        device->bindBindGroup(group.value());
        device->bindVertexBuffer(vbo);
        device->bindIndexBuffer(ibo, render::IndexType::U16);

        // A 3x3 grid of cubes, each spun differently — per-object push constant.
        for (int gy = -1; gy <= 1; ++gy) {
            for (int gx = -1; gx <= 1; ++gx) {
                const math::Mat4 translate =
                    math::Mat4::translation({static_cast<f32>(gx) * 1.5f, static_cast<f32>(gy) * 1.5f, 0.0f});
                const math::Mat4 spin = math::toMat4(
                    math::Quat::fromEuler(t + static_cast<f32>(gx), t + static_cast<f32>(gy), 0.0f));
                const math::Mat4 model = translate * spin;
                device->pushConstants(model.e, sizeof(model.e));
                device->drawIndexed(static_cast<u32>(cube.indices.size()));
            }
        }
        device->endRenderPass();
        device->endFrame();
    }

    device->waitIdle();
    device->destroyBindGroup(group.value());
    device->destroyPipeline(pipeline.value());
    device->destroyTexture(checker);
    device->destroyBuffer(camUbo);
    device->destroyBuffer(ibo);
    device->destroyBuffer(vbo);
    SUCCEED("drew a 3x3 grid of push-constant cubes for 30 frames");
}

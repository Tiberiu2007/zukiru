#include <zuki/renderer/renderer.hpp>

#include <zuki/ecs/world.hpp>
#include <zuki/render/rhi.hpp>
#include <zuki/scene/components.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace zuki;

namespace {

// A Device that records the recording calls renderMeshes makes, so we can assert
// draw order and state-change minimization without a GPU.
class RecordingDevice final : public render::Device {
public:
    std::vector<std::string> log;

    void bindPipeline(render::PipelineHandle p) override {
        log.push_back("pipeline:" + std::to_string(p.id));
    }
    void bindBindGroup(render::BindGroupHandle g) override {
        log.push_back("group:" + std::to_string(g.id));
    }
    void bindVertexBuffer(render::BufferHandle b) override {
        log.push_back("vbo:" + std::to_string(b.id));
    }
    void bindIndexBuffer(render::BufferHandle b, render::IndexType) override {
        log.push_back("ibo:" + std::to_string(b.id));
    }
    void pushConstants(const void*, u32) override { log.push_back("push"); }
    void draw(u32 count, u32) override { log.push_back("draw:" + std::to_string(count)); }
    void drawIndexed(u32 count, u32, i32) override {
        log.push_back("drawI:" + std::to_string(count));
    }

    [[nodiscard]] usize countPrefix(std::string_view prefix) const {
        return static_cast<usize>(std::count_if(log.begin(), log.end(), [&](const std::string& s) {
            return s.rfind(prefix, 0) == 0;
        }));
    }

    // --- Everything else is an unused no-op ------------------------------
    render::BufferHandle createBuffer(render::BufferKind, const void*, usize) override { return {}; }
    void destroyBuffer(render::BufferHandle) override {}
    void updateBuffer(render::BufferHandle, const void*, usize) override {}
    render::TextureHandle createTexture(u32, u32, const void*) override { return {}; }
    void destroyTexture(render::TextureHandle) override {}
    Result<render::PipelineHandle> createPipeline(const render::PipelineDesc&) override {
        return Ok(render::PipelineHandle{});
    }
    void destroyPipeline(render::PipelineHandle) override {}
    Result<render::BindGroupHandle> createBindGroup(
        render::PipelineHandle, std::span<const render::BindGroupEntry>) override {
        return Ok(render::BindGroupHandle{});
    }
    void destroyBindGroup(render::BindGroupHandle) override {}
    render::RenderTargetHandle createRenderTarget(const render::RenderTargetDesc&) override {
        return {};
    }
    void destroyRenderTarget(render::RenderTargetHandle) override {}
    [[nodiscard]] render::TextureHandle renderTargetTexture(
        render::RenderTargetHandle) const override {
        return {};
    }
    bool beginFrame() override { return true; }
    void endFrame() override {}
    void beginRenderPass(render::RenderTargetHandle) override {}
    void beginSwapchainPass() override {}
    void endRenderPass() override {}
    void setClearColor(render::Color) override {}
    void resize(u32, u32) override {}
    void waitIdle() override {}
    [[nodiscard]] render::Backend backend() const override { return render::Backend::Vulkan; }
    [[nodiscard]] std::string_view deviceName() const override { return "recording"; }
};

// A MeshRenderer with fabricated handles (no GPU objects behind them).
renderer::MeshRenderer makeRenderer(u32 pipeline, u32 group, u32 vbo, u32 ibo, u32 count) {
    return {.mesh = {.vertexBuffer = render::BufferHandle{vbo},
                     .indexBuffer = render::BufferHandle{ibo},
                     .count = count},
            .pipeline = render::PipelineHandle{pipeline},
            .bindGroup = render::BindGroupHandle{group}};
}

}  // namespace

TEST_CASE("Mesh reports indexed/valid from its handles", "[renderer][mesh]") {
    renderer::Mesh empty;
    REQUIRE_FALSE(empty.valid());
    REQUIRE_FALSE(empty.indexed());

    const renderer::Mesh indexed{.vertexBuffer = render::BufferHandle{1},
                                 .indexBuffer = render::BufferHandle{2},
                                 .count = 36};
    REQUIRE(indexed.valid());
    REQUIRE(indexed.indexed());

    const renderer::Mesh nonIndexed{.vertexBuffer = render::BufferHandle{1}, .count = 3};
    REQUIRE(nonIndexed.valid());
    REQUIRE_FALSE(nonIndexed.indexed());
}

TEST_CASE("renderMeshes draws each entity and minimizes state changes",
          "[renderer][system]") {
    ecs::World world;
    // Three entities share one pipeline/group/mesh; a fourth uses another set.
    for (int i = 0; i < 3; ++i) {
        (void)world.create(scene::WorldTransform{}, makeRenderer(1, 1, 10, 11, 36));
    }
    (void)world.create(scene::WorldTransform{}, makeRenderer(2, 2, 20, 21, 24));

    RecordingDevice device;
    renderer::renderMeshes(device, world);

    // One draw (+ push) per entity.
    REQUIRE(device.countPrefix("drawI:") == 4);
    REQUIRE(device.countPrefix("push") == 4);
    // Shared state bound once per group of consecutive identical draws.
    REQUIRE(device.countPrefix("pipeline:") == 2);
    REQUIRE(device.countPrefix("group:") == 2);
    REQUIRE(device.countPrefix("vbo:") == 2);
    REQUIRE(device.countPrefix("ibo:") == 2);
    REQUIRE(device.log.front() == "pipeline:1");
}

TEST_CASE("renderMeshes skips empty meshes and handles non-indexed draws",
          "[renderer][system]") {
    ecs::World world;
    // An entity whose mesh has no vertex buffer is skipped entirely.
    (void)world.create(scene::WorldTransform{}, renderer::MeshRenderer{});
    // A non-indexed mesh draws with draw(), not drawIndexed(), and binds no ibo.
    (void)world.create(scene::WorldTransform{},
                       renderer::MeshRenderer{.mesh = {.vertexBuffer = render::BufferHandle{30},
                                                       .count = 3},
                                              .pipeline = render::PipelineHandle{3}});

    RecordingDevice device;
    renderer::renderMeshes(device, world);

    REQUIRE(device.countPrefix("draw:") == 1);   // one non-indexed draw
    REQUIRE(device.countPrefix("drawI:") == 0);  // nothing indexed
    REQUIRE(device.countPrefix("ibo:") == 0);    // no index buffer bound
    REQUIRE(device.countPrefix("vbo:") == 1);    // only the valid entity
}

TEST_CASE("an entity without a MeshRenderer is not drawn", "[renderer][system]") {
    ecs::World world;
    (void)world.create(scene::WorldTransform{});  // a transform-only node (e.g. a pivot)
    (void)world.create(scene::WorldTransform{}, makeRenderer(1, 1, 10, 11, 36));

    RecordingDevice device;
    renderer::renderMeshes(device, world);
    REQUIRE(device.countPrefix("drawI:") == 1);  // only the one with a MeshRenderer
}

#include <zukiru/render/render_graph.hpp>

#include <zukiru/render/rhi.hpp>

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace zukiru;
using namespace zukiru::render;

namespace {

// A do-nothing Device so we can drive RenderGraph::execute without a GPU.
class StubDevice final : public Device {
public:
    BufferHandle createBuffer(BufferKind, const void*, usize) override { return {}; }
    void destroyBuffer(BufferHandle) override {}
    void updateBuffer(BufferHandle, const void*, usize) override {}
    TextureHandle createTexture(u32, u32, const void*) override { return {}; }
    void destroyTexture(TextureHandle) override {}
    Result<PipelineHandle> createPipeline(const PipelineDesc&) override { return Ok(PipelineHandle{}); }
    void destroyPipeline(PipelineHandle) override {}
    Result<BindGroupHandle> createBindGroup(PipelineHandle,
                                            std::span<const BindGroupEntry>) override {
        return Ok(BindGroupHandle{});
    }
    void destroyBindGroup(BindGroupHandle) override {}
    bool beginFrame() override { return true; }
    void endFrame() override {}
    void bindPipeline(PipelineHandle) override {}
    void bindBindGroup(BindGroupHandle) override {}
    void bindVertexBuffer(BufferHandle) override {}
    void bindIndexBuffer(BufferHandle, IndexType) override {}
    void draw(u32, u32) override {}
    void drawIndexed(u32, u32, i32) override {}
    void setClearColor(Color) override {}
    void resize(u32, u32) override {}
    void waitIdle() override {}
    [[nodiscard]] Backend backend() const override { return Backend::Vulkan; }
    [[nodiscard]] std::string_view deviceName() const override { return "stub"; }
};

}  // namespace

TEST_CASE("compile orders passes by dependency, not declaration order", "[render][graph]") {
    RenderGraph graph;
    const RgResource shadow = graph.createResource("shadow");
    const RgResource backbuffer = graph.importResource("backbuffer");

    // Declare the consumer first; the producer must still run before it.
    graph.addPass("opaque").reads(shadow).writes(backbuffer);  // pass 0
    graph.addPass("shadow").writes(shadow);                    // pass 1

    const Result<CompiledGraph> compiled = graph.compile();
    REQUIRE(compiled.isOk());
    REQUIRE(compiled.value().order.size() == 2);
    REQUIRE(compiled.value().order[0] == 1);  // shadow producer first
    REQUIRE(compiled.value().order[1] == 0);  // opaque consumer second
}

TEST_CASE("compile culls passes whose output nothing consumes", "[render][graph]") {
    RenderGraph graph;
    const RgResource backbuffer = graph.importResource("backbuffer");
    const RgResource orphan = graph.createResource("orphan");

    graph.addPass("draw").writes(backbuffer);   // pass 0 — live (writes imported)
    graph.addPass("wasted").writes(orphan);      // pass 1 — dead (orphan unread)

    const Result<CompiledGraph> compiled = graph.compile();
    REQUIRE(compiled.isOk());
    REQUIRE(compiled.value().order.size() == 1);
    REQUIRE(compiled.value().order[0] == 0);
}

TEST_CASE("compile keeps a transitive chain feeding the backbuffer", "[render][graph]") {
    RenderGraph graph;
    const RgResource a = graph.createResource("a");
    const RgResource b = graph.createResource("b");
    const RgResource backbuffer = graph.importResource("backbuffer");

    graph.addPass("makeA").writes(a);               // 0
    graph.addPass("aToB").reads(a).writes(b);        // 1
    graph.addPass("present").reads(b).writes(backbuffer);  // 2

    const Result<CompiledGraph> compiled = graph.compile();
    REQUIRE(compiled.isOk());
    REQUIRE(compiled.value().order.size() == 3);
    REQUIRE(compiled.value().order[0] == 0);
    REQUIRE(compiled.value().order[1] == 1);
    REQUIRE(compiled.value().order[2] == 2);
}

TEST_CASE("compile rejects a dependency cycle", "[render][graph]") {
    RenderGraph graph;
    const RgResource r0 = graph.createResource("r0");
    const RgResource r1 = graph.createResource("r1");
    const RgResource backbuffer = graph.importResource("backbuffer");

    graph.addPass("p0").reads(r1).writes(r0).writes(backbuffer);
    graph.addPass("p1").reads(r0).writes(r1);

    REQUIRE(graph.compile().isErr());
}

TEST_CASE("compile rejects a dangling read of an unwritten resource", "[render][graph]") {
    RenderGraph graph;
    const RgResource missing = graph.createResource("missing");
    const RgResource backbuffer = graph.importResource("backbuffer");

    graph.addPass("draw").reads(missing).writes(backbuffer);

    REQUIRE(graph.compile().isErr());
}

TEST_CASE("reading an imported resource needs no writer", "[render][graph]") {
    RenderGraph graph;
    const RgResource external = graph.importResource("externalInput");
    const RgResource backbuffer = graph.importResource("backbuffer");

    graph.addPass("blit").reads(external).writes(backbuffer);

    REQUIRE(graph.compile().isOk());
}

TEST_CASE("execute runs live passes in order and skips dead ones", "[render][graph]") {
    RenderGraph graph;
    const RgResource shadow = graph.createResource("shadow");
    const RgResource orphan = graph.createResource("orphan");
    const RgResource backbuffer = graph.importResource("backbuffer");

    std::vector<int> ran;
    graph.addPass("opaque").reads(shadow).writes(backbuffer).setExecute(
        [&](PassContext&) { ran.push_back(2); });                         // 0
    graph.addPass("shadow").writes(shadow).setExecute(
        [&](PassContext&) { ran.push_back(1); });                         // 1
    graph.addPass("dead").writes(orphan).setExecute(
        [&](PassContext&) { ran.push_back(99); });                        // 2

    StubDevice device;
    graph.execute(device);

    REQUIRE(ran.size() == 2);
    REQUIRE(ran[0] == 1);  // shadow ran first
    REQUIRE(ran[1] == 2);  // opaque second; the dead pass never ran
}

#include <zukiru/render/rhi.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zukiru;
using namespace zukiru::render;

TEST_CASE("DeviceConfig defaults to a vsynced Vulkan device", "[render][rhi]") {
    const DeviceConfig cfg;
    REQUIRE(cfg.backend == Backend::Vulkan);
    REQUIRE(cfg.vsync);
    REQUIRE_FALSE(cfg.enableValidation);
}

TEST_CASE("Color compares by value and defaults to opaque black", "[render][rhi]") {
    const Color black;
    REQUIRE(black == Color{0.0f, 0.0f, 0.0f, 1.0f});
    REQUIRE_FALSE(black == Color{1.0f, 1.0f, 1.0f, 1.0f});
}

TEST_CASE("resource handles default to invalid", "[render][rhi]") {
    REQUIRE_FALSE(BufferHandle{}.valid());
    REQUIRE_FALSE(PipelineHandle{}.valid());
    REQUIRE(BufferHandle{7}.valid());
    REQUIRE(BufferHandle{7} == BufferHandle{7});
    REQUIRE_FALSE(BufferHandle{7} == BufferHandle{8});
}

TEST_CASE("a vertex layout describes interleaved attributes", "[render][rhi]") {
    VertexLayout layout;
    layout.stride = 20;
    layout.attributes = {
        {.location = 0, .format = VertexFormat::Float32x2, .offset = 0},
        {.location = 1, .format = VertexFormat::Float32x3, .offset = 8},
    };
    REQUIRE(layout.attributes.size() == 2);
    REQUIRE(layout.attributes[1].location == 1);
    REQUIRE(layout.attributes[1].offset == 8);

    PipelineDesc desc;
    desc.vertexLayout = layout;
    desc.topology = PrimitiveTopology::TriangleList;
    REQUIRE(desc.vertexLayout.stride == 20);
    REQUIRE(desc.vertexSpirv.empty());  // unset span is empty
}

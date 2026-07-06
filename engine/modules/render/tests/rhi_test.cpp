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
    REQUIRE_FALSE(TextureHandle{}.valid());
    REQUIRE_FALSE(BindGroupHandle{}.valid());
    REQUIRE_FALSE(RenderTargetHandle{}.valid());
    REQUIRE(BufferHandle{7}.valid());
    REQUIRE(BufferHandle{7} == BufferHandle{7});
    REQUIRE_FALSE(BufferHandle{7} == BufferHandle{8});
    REQUIRE(RenderTargetHandle{3}.valid());
    REQUIRE(RenderTargetHandle{3} == RenderTargetHandle{3});
}

TEST_CASE("a render target defaults to zero-sized with an opaque-black clear",
          "[render][rhi]") {
    const RenderTargetDesc desc;
    REQUIRE(desc.width == 0);
    REQUIRE(desc.height == 0);
    REQUIRE(desc.clearColor == Color{0.0f, 0.0f, 0.0f, 1.0f});
}

TEST_CASE("a pipeline declares resource bindings; a bind group fills them", "[render][rhi]") {
    PipelineDesc desc;
    desc.bindings = {BindingType::UniformBuffer, BindingType::Texture};
    REQUIRE(desc.bindings.size() == 2);
    REQUIRE(desc.bindings[0] == BindingType::UniformBuffer);

    // A uniform-buffer entry carries a buffer; a texture entry carries a texture.
    const BindGroupEntry uniform{.binding = 0, .buffer = BufferHandle{3}, .texture = {}};
    const BindGroupEntry sampled{.binding = 1, .buffer = {}, .texture = TextureHandle{4}};
    REQUIRE(uniform.buffer.valid());
    REQUIRE_FALSE(uniform.texture.valid());
    REQUIRE(sampled.texture.valid());
    REQUIRE_FALSE(sampled.buffer.valid());
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

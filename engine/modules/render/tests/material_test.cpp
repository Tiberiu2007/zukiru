#include <zukiru/render/material.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstring>

using namespace zukiru;
using namespace zukiru::render;

namespace {

// Read a float back out of a packed uniform block at a byte offset.
f32 floatAt(std::span<const u8> data, usize offset) {
    f32 value = 0.0f;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

}  // namespace

TEST_CASE("std140 sizes and alignments", "[render][material]") {
    REQUIRE(paramSize(ParamType::Float) == 4);
    REQUIRE(paramSize(ParamType::Vec3) == 12);
    REQUIRE(paramSize(ParamType::Vec4) == 16);
    REQUIRE(paramSize(ParamType::Mat4) == 64);
    REQUIRE(paramAlign(ParamType::Vec3) == 16);  // vec3 occupies 12 but aligns to 16
    REQUIRE(paramAlign(ParamType::Vec2) == 8);
}

TEST_CASE("MaterialLayout lays out members with std140 offsets", "[render][material]") {
    MaterialLayout layout;
    layout.addFloat("a").addVec3("b").addVec2("c").addMat4("d");

    REQUIRE(layout.findParam("a")->offset == 0);
    REQUIRE(layout.findParam("b")->offset == 16);  // vec3 bumped from 4 to 16
    REQUIRE(layout.findParam("c")->offset == 32);  // vec2 after vec3 (28 -> 32)
    REQUIRE(layout.findParam("d")->offset == 48);  // mat4 after vec2 (40 -> 48)
    REQUIRE(layout.uniformSize() == 112);          // 48 + 64, padded to 16
    REQUIRE(layout.findParam("missing") == nullptr);
}

TEST_CASE("MaterialLayout maps a matching uniform block + texture bindings",
          "[render][material]") {
    MaterialLayout layout;
    layout.addMat4("mvp").addMat4("model").addTexture("albedo").addTexture("normal");

    REQUIRE(layout.hasUniformBlock());
    REQUIRE(layout.uniformSize() == 128);  // two mat4s
    REQUIRE(layout.bindings().size() == 3);
    REQUIRE(layout.bindings()[0] == BindingType::UniformBuffer);
    REQUIRE(layout.bindings()[1] == BindingType::Texture);
    REQUIRE(layout.textureBinding("albedo") == 1);  // after the uniform block
    REQUIRE(layout.textureBinding("normal") == 2);
    REQUIRE(layout.textureBinding("missing") == -1);
}

TEST_CASE("a texture-only layout starts bindings at 0", "[render][material]") {
    MaterialLayout layout;
    layout.addTexture("t0").addTexture("t1");
    REQUIRE_FALSE(layout.hasUniformBlock());
    REQUIRE(layout.uniformSize() == 0);
    REQUIRE(layout.textureBinding("t0") == 0);
    REQUIRE(layout.textureBinding("t1") == 1);
    REQUIRE(layout.bindings().size() == 2);
}

TEST_CASE("MaterialParams packs values at the layout's offsets", "[render][material]") {
    MaterialLayout layout;
    layout.addVec4("color").addFloat("intensity");
    MaterialParams params{layout};
    REQUIRE(params.size() == 32);  // vec4 (16) + float (16, padded)

    params.setVec4("color", {0.1f, 0.2f, 0.3f, 0.4f}).setFloat("intensity", 2.5f);
    const std::span<const u8> data = params.data();
    REQUIRE(floatAt(data, 0) == 0.1f);
    REQUIRE(floatAt(data, 12) == 0.4f);
    REQUIRE(floatAt(data, 16) == 2.5f);  // float sits after the vec4
}

TEST_CASE("MaterialParams round-trips a mat4", "[render][material]") {
    MaterialLayout layout;
    layout.addMat4("m");
    MaterialParams params{layout};

    math::Mat4 m = math::Mat4::identity();
    m.e[3] = 7.0f;
    m.e[15] = 9.0f;
    params.setMat4("m", m);
    REQUIRE(floatAt(params.data(), 3 * sizeof(f32)) == 7.0f);
    REQUIRE(floatAt(params.data(), 15 * sizeof(f32)) == 9.0f);
}

TEST_CASE("MaterialParams ignores unknown names and type mismatches", "[render][material]") {
    MaterialLayout layout;
    layout.addFloat("x");
    MaterialParams params{layout};

    params.setFloat("x", 3.0f);
    params.setFloat("nope", 99.0f);      // unknown name — no effect
    params.setVec4("x", {1, 1, 1, 1});   // wrong type for "x" — no effect
    REQUIRE(floatAt(params.data(), 0) == 3.0f);
}

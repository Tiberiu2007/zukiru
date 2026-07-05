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

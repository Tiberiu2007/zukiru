#include <zukiru/platform/window.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using namespace zukiru;
using namespace zukiru::platform;

TEST_CASE("WindowConfig has sensible defaults", "[platform][window]") {
    const WindowConfig cfg;
    REQUIRE(cfg.title == "Zukiru");
    REQUIRE(cfg.width == 1280);
    REQUIRE(cfg.height == 720);
    REQUIRE(cfg.mode == WindowMode::Windowed);
    REQUIRE(cfg.resizable);
}

TEST_CASE("Window is an abstract interface", "[platform][window]") {
    STATIC_REQUIRE(std::is_abstract_v<Window>);
    STATIC_REQUIRE(std::has_virtual_destructor_v<Window>);
}

TEST_CASE("WindowExtent compares by value", "[platform][window]") {
    STATIC_REQUIRE(WindowExtent{800, 600} == WindowExtent{800, 600});
    STATIC_REQUIRE_FALSE(WindowExtent{800, 600} == WindowExtent{640, 480});
}

// createWindow() intentionally errors until a backend is implemented (ADR 0003).
// This documents the current state; update it when a backend lands.
TEST_CASE("createWindow reports no backend yet", "[platform][window]") {
    auto result = createWindow(WindowConfig{});
    REQUIRE(result.isErr());
}

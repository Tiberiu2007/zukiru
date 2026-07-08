#include <zuki/platform/window.hpp>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <type_traits>

using namespace zuki;
using namespace zuki::platform;

TEST_CASE("WindowConfig has sensible defaults", "[platform][window]") {
    const WindowConfig cfg;
    REQUIRE(cfg.title == "Zuki");
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

TEST_CASE("at least one windowing backend is compiled in", "[platform][window]") {
    const WindowBackends backends = compiledWindowBackends();
    // This build targets Linux, so X11 and/or Wayland should be present.
    REQUIRE((backends.x11 || backends.wayland));
}

TEST_CASE("WindowEvent defaults are inert", "[platform][window]") {
    const WindowEvent e;
    REQUIRE(e.key == Key::Unknown);
    REQUIRE(e.mods == KeyMods::None);
    REQUIRE(e.width == 0);
}

// Real window creation needs a display and a window manager, so it is hidden by
// default (Catch2 runs tests tagged with a leading '.' only when asked). Run it
// on a machine with a display via:  zuki_platform_tests "[.window]"
TEST_CASE("open a real window and pump events", "[.window]") {
    Result<std::unique_ptr<Window>> result =
        createWindow({.title = "Zuki window test", .width = 640, .height = 480});
    REQUIRE(result.isOk());

    std::unique_ptr<Window>& window = result.value();
    REQUIRE(window->nativeHandle() != nullptr);
    REQUIRE(window->nativeDisplay() != nullptr);
    REQUIRE_FALSE(window->shouldClose());

    window->setTitle("Zuki window test (renamed)");
    for (int i = 0; i < 5; ++i) {
        window->pollEvents();
    }

    const WindowExtent extent = window->extent();
    REQUIRE(extent.width > 0);
    REQUIRE(extent.height > 0);
}

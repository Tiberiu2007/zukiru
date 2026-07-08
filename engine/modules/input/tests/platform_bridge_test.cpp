#include <zuki/input/platform_bridge.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace zuki;
using namespace zuki::input;

namespace {

platform::WindowEvent keyEvent(platform::EventType type, platform::Key key,
                               platform::KeyMods mods = platform::KeyMods::None) {
    platform::WindowEvent e;
    e.type = type;
    e.key = key;
    e.mods = mods;
    return e;
}

}  // namespace

TEST_CASE("platform enums translate to input enums", "[input][bridge]") {
    REQUIRE(toInputKey(platform::Key::A) == Key::A);
    REQUIRE(toInputKey(platform::Key::Escape) == Key::Escape);
    REQUIRE(toInputKey(platform::Key::Unknown) == Key::Unknown);
    REQUIRE(toInputMouseButton(platform::MouseButton::Right) == MouseButton::Right);
    REQUIRE(toInputMods(platform::KeyMods::Shift | platform::KeyMods::Control) ==
            (KeyMods::Shift | KeyMods::Control));
}

TEST_CASE("key events feed into InputState with edges", "[input][bridge]") {
    InputState state;
    platform::WindowEvent events[] = {
        keyEvent(platform::EventType::KeyDown, platform::Key::Space),
    };

    state.beginFrame();
    for (const auto& e : events) feedEvent(state, e);

    REQUIRE(state.keyDown(Key::Space));
    REQUIRE(state.keyPressed(Key::Space));

    // Next frame, release.
    state.beginFrame();
    feedEvent(state, keyEvent(platform::EventType::KeyUp, platform::Key::Space));
    REQUIRE_FALSE(state.keyDown(Key::Space));
    REQUIRE(state.keyReleased(Key::Space));
}

TEST_CASE("modifiers propagate through the bridge", "[input][bridge]") {
    InputState state;
    state.beginFrame();
    feedEvent(state, keyEvent(platform::EventType::KeyDown, platform::Key::S,
                              platform::KeyMods::Control));
    REQUIRE(state.keyDown(Key::S));
    REQUIRE(hasMods(state.modifiers(), KeyMods::Control));
}

TEST_CASE("mouse move / button / scroll events feed through", "[input][bridge]") {
    InputState state;
    state.beginFrame();

    platform::WindowEvent move;
    move.type = platform::EventType::MouseMove;
    move.x = 120.0f;
    move.y = 80.0f;
    feedEvent(state, move);

    platform::WindowEvent button;
    button.type = platform::EventType::MouseButtonDown;
    button.button = platform::MouseButton::Left;
    feedEvent(state, button);

    platform::WindowEvent scroll;
    scroll.type = platform::EventType::MouseScroll;
    scroll.y = 1.0f;
    feedEvent(state, scroll);

    REQUIRE(state.mousePosition() == Vec2{120.0f, 80.0f});
    REQUIRE(state.mouseButtonPressed(MouseButton::Left));
    REQUIRE(state.scrollDelta() == Vec2{0.0f, 1.0f});
}

TEST_CASE("non-input events are ignored by the bridge", "[input][bridge]") {
    InputState state;
    state.beginFrame();

    platform::WindowEvent resize;
    resize.type = platform::EventType::WindowResize;
    resize.width = 800;
    resize.height = 600;
    feedEvent(state, resize);  // must not throw or affect input

    platform::WindowEvent close;
    close.type = platform::EventType::WindowClose;
    feedEvent(state, close);

    REQUIRE(state.mousePosition() == Vec2{0.0f, 0.0f});
    REQUIRE_FALSE(state.keyDown(Key::Space));
}

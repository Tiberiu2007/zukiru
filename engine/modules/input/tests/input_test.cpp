#include <zukiru/input/input.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace zukiru;
using namespace zukiru::input;
using Catch::Matchers::WithinAbs;

TEST_CASE("key edges: down / pressed / released across frames", "[input]") {
    InputState state;

    // Frame 1: press Space.
    state.beginFrame();
    state.onKey(Key::Space, true);
    REQUIRE(state.keyDown(Key::Space));
    REQUIRE(state.keyPressed(Key::Space));   // down edge this frame
    REQUIRE_FALSE(state.keyReleased(Key::Space));

    // Frame 2: still held -> down but no longer a fresh press.
    state.beginFrame();
    REQUIRE(state.keyDown(Key::Space));
    REQUIRE_FALSE(state.keyPressed(Key::Space));

    // Frame 3: release -> released edge, no longer down.
    state.beginFrame();
    state.onKey(Key::Space, false);
    REQUIRE_FALSE(state.keyDown(Key::Space));
    REQUIRE(state.keyReleased(Key::Space));

    // Frame 4: settled.
    state.beginFrame();
    REQUIRE_FALSE(state.keyReleased(Key::Space));
}

TEST_CASE("mouse position, delta and scroll accumulation", "[input]") {
    InputState state;

    state.beginFrame();
    state.onMouseMove(100.0f, 50.0f);
    // First frame delta is measured from (0,0) baseline.
    REQUIRE(state.mousePosition() == Vec2{100.0f, 50.0f});

    state.beginFrame();  // baseline now (100,50)
    state.onMouseMove(110.0f, 45.0f);
    state.onScroll(0.0f, 1.0f);
    state.onScroll(0.0f, 2.0f);  // accumulates within the frame
    REQUIRE(state.mouseDelta() == Vec2{10.0f, -5.0f});
    REQUIRE(state.scrollDelta() == Vec2{0.0f, 3.0f});

    // beginFrame resets scroll but keeps position.
    state.beginFrame();
    REQUIRE(state.scrollDelta() == Vec2{0.0f, 0.0f});
    REQUIRE(state.mouseDelta() == Vec2{0.0f, 0.0f});
    REQUIRE(state.mousePosition() == Vec2{110.0f, 45.0f});
}

TEST_CASE("mouse and gamepad buttons report edges", "[input]") {
    InputState state;

    state.beginFrame();
    state.onMouseButton(MouseButton::Left, true);
    state.onGamepadButton(GamepadButton::A, true);
    REQUIRE(state.mouseButtonPressed(MouseButton::Left));
    REQUIRE(state.gamepadButtonPressed(GamepadButton::A));

    state.beginFrame();
    REQUIRE(state.mouseButtonDown(MouseButton::Left));
    REQUIRE_FALSE(state.mouseButtonPressed(MouseButton::Left));
    REQUIRE_FALSE(state.gamepadButtonPressed(GamepadButton::A));
}

TEST_CASE("gamepad axis and connection state", "[input]") {
    InputState state;
    REQUIRE_FALSE(state.gamepadConnected());

    state.setGamepadConnected(true);
    state.onGamepadAxis(GamepadAxis::LeftX, 0.5f);
    REQUIRE(state.gamepadConnected());
    REQUIRE_THAT(state.gamepadAxis(GamepadAxis::LeftX), WithinAbs(0.5f, 1e-6f));
}

TEST_CASE("reset clears all device state", "[input]") {
    InputState state;
    state.onKey(Key::W, true);
    state.onMouseMove(10.0f, 10.0f);
    state.reset();
    REQUIRE_FALSE(state.keyDown(Key::W));
    REQUIRE(state.mousePosition() == Vec2{0.0f, 0.0f});
}

TEST_CASE("action fires when any bound button fires", "[input]") {
    ActionMap map;
    map.bindAction("Jump", ButtonBinding::key(Key::Space));
    map.bindAction("Jump", ButtonBinding::pad(GamepadButton::A));
    REQUIRE(map.hasAction("Jump"));

    InputState state;
    state.beginFrame();
    REQUIRE_FALSE(map.actionActive("Jump", state));

    // Keyboard binding.
    state.onKey(Key::Space, true);
    REQUIRE(map.actionActive("Jump", state));
    REQUIRE(map.actionPressed("Jump", state));

    // Second frame: the gamepad binding also drives the same action.
    state.beginFrame();
    state.onKey(Key::Space, false);
    state.onGamepadButton(GamepadButton::A, true);
    REQUIRE(map.actionActive("Jump", state));
    REQUIRE(map.actionReleased("Jump", state));  // the Space binding released
}

TEST_CASE("modifier-gated binding requires the modifier held", "[input]") {
    ActionMap map;
    map.bindAction("Save", ButtonBinding::key(Key::S).withMods(KeyMods::Control));

    InputState state;
    state.beginFrame();
    state.onKey(Key::S, true, KeyMods::None);
    REQUIRE_FALSE(map.actionActive("Save", state));  // no Ctrl

    state.beginFrame();
    state.onKey(Key::S, true, KeyMods::Control);
    REQUIRE(map.actionActive("Save", state));
}

TEST_CASE("digital axis: positive/negative keys and cancellation", "[input]") {
    ActionMap map;
    map.bindAxis("MoveX", AxisBinding::keys(Key::D, Key::A));  // +D, -A
    REQUIRE(map.hasAxis("MoveX"));

    InputState state;
    state.beginFrame();
    REQUIRE_THAT(map.axisValue("MoveX", state), WithinAbs(0.0f, 1e-6f));

    state.onKey(Key::D, true);
    REQUIRE_THAT(map.axisValue("MoveX", state), WithinAbs(1.0f, 1e-6f));

    state.onKey(Key::A, true);  // both held -> cancel
    REQUIRE_THAT(map.axisValue("MoveX", state), WithinAbs(0.0f, 1e-6f));

    state.onKey(Key::D, false);
    REQUIRE_THAT(map.axisValue("MoveX", state), WithinAbs(-1.0f, 1e-6f));
}

TEST_CASE("gamepad axis: dead zone, scale and clamping", "[input]") {
    ActionMap map;
    map.bindAxis("MoveX", AxisBinding::gamepad(GamepadAxis::LeftX, 1.0f, 0.2f));

    InputState state;
    state.beginFrame();

    state.onGamepadAxis(GamepadAxis::LeftX, 0.1f);  // inside dead zone
    REQUIRE_THAT(map.axisValue("MoveX", state), WithinAbs(0.0f, 1e-6f));

    state.onGamepadAxis(GamepadAxis::LeftX, 0.6f);  // outside -> passes through
    REQUIRE_THAT(map.axisValue("MoveX", state), WithinAbs(0.6f, 1e-6f));

    // Digital + analog on the same action sum and clamp to [-1, 1].
    map.bindAxis("MoveX", AxisBinding::keys(Key::D, Key::A));
    state.onKey(Key::D, true);
    REQUIRE_THAT(map.axisValue("MoveX", state), WithinAbs(1.0f, 1e-6f));  // 0.6 + 1 -> clamp
}

TEST_CASE("inverted axis via negative scale", "[input]") {
    ActionMap map;
    map.bindAxis("LookY", AxisBinding::gamepad(GamepadAxis::RightY, -1.0f, 0.0f));

    InputState state;
    state.beginFrame();
    state.onGamepadAxis(GamepadAxis::RightY, 0.5f);
    REQUIRE_THAT(map.axisValue("LookY", state), WithinAbs(-0.5f, 1e-6f));
}

TEST_CASE("unbound actions read as inactive / zero", "[input]") {
    ActionMap map;
    InputState state;
    state.beginFrame();
    REQUIRE_FALSE(map.actionActive("Nope", state));
    REQUIRE_THAT(map.axisValue("Nope", state), WithinAbs(0.0f, 1e-6f));

    map.bindAction("Fire", ButtonBinding::mouse(MouseButton::Left));
    map.clearAction("Fire");
    REQUIRE_FALSE(map.hasAction("Fire"));
}

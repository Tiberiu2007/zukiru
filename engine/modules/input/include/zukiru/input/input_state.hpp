// InputState — the snapshot of every device this frame, plus one-frame edge info.
//
// It is a *sink*, not a poller: a platform backend (or a test) pushes raw device
// events into it via the on*() methods; game code queries it. This keeps input
// decoupled from any specific windowing backend (see docs/adr/0003).
//
// Per-frame flow:
//   state.beginFrame();          // roll "current" into "previous", clear deltas
//   /* pump OS events -> state.onKey(...), state.onMouseMove(...), ... */
//   if (state.keyPressed(Key::Space)) jump();   // true only on the down edge
//
// `*Down` = held now. `*Pressed` = went down this frame. `*Released` = went up
// this frame. Edges are derived from the previous-frame snapshot, so beginFrame()
// must be called exactly once per frame before events are pumped.
#pragma once

#include <zukiru/core/types.hpp>
#include <zukiru/input/key_code.hpp>

#include <array>

namespace zukiru::input {

// A 2D value used for mouse position, movement delta and scroll offset.
struct Vec2 {
    f32 x = 0.0f;
    f32 y = 0.0f;

    friend constexpr bool operator==(Vec2, Vec2) = default;
};

class InputState {
public:
    InputState() = default;

    // Advance to a new frame: the current button/key state becomes the "previous"
    // baseline for edge detection, and per-frame accumulators (mouse delta, scroll)
    // are reset. Call once per frame, before pumping events.
    void beginFrame() {
        keysPrev_ = keys_;
        mouseButtonsPrev_ = mouseButtons_;
        padButtonsPrev_ = padButtons_;
        mousePrev_ = mouse_;
        scroll_ = Vec2{};
    }

    // --- Event injection (called by a backend / test) --------------------

    void onKey(Key key, bool down, KeyMods mods = KeyMods::None) {
        keys_[index(key)] = down;
        mods_ = mods;
    }
    void onMouseButton(MouseButton button, bool down) {
        mouseButtons_[index(button)] = down;
    }
    // Absolute cursor position, in window pixels. Delta is derived from the
    // position at frame start.
    void onMouseMove(f32 x, f32 y) { mouse_ = Vec2{x, y}; }
    // Scroll offset for this frame; multiple events accumulate.
    void onScroll(f32 dx, f32 dy) {
        scroll_.x += dx;
        scroll_.y += dy;
    }
    void onGamepadButton(GamepadButton button, bool down) {
        padButtons_[index(button)] = down;
    }
    void onGamepadAxis(GamepadAxis axis, f32 value) { padAxes_[index(axis)] = value; }
    void setGamepadConnected(bool connected) { padConnected_ = connected; }

    // --- Keyboard queries ------------------------------------------------

    [[nodiscard]] bool keyDown(Key key) const { return keys_[index(key)]; }
    [[nodiscard]] bool keyPressed(Key key) const {
        return keys_[index(key)] && !keysPrev_[index(key)];
    }
    [[nodiscard]] bool keyReleased(Key key) const {
        return !keys_[index(key)] && keysPrev_[index(key)];
    }
    [[nodiscard]] KeyMods modifiers() const noexcept { return mods_; }

    // --- Mouse queries ---------------------------------------------------

    [[nodiscard]] bool mouseButtonDown(MouseButton b) const { return mouseButtons_[index(b)]; }
    [[nodiscard]] bool mouseButtonPressed(MouseButton b) const {
        return mouseButtons_[index(b)] && !mouseButtonsPrev_[index(b)];
    }
    [[nodiscard]] bool mouseButtonReleased(MouseButton b) const {
        return !mouseButtons_[index(b)] && mouseButtonsPrev_[index(b)];
    }
    [[nodiscard]] Vec2 mousePosition() const noexcept { return mouse_; }
    [[nodiscard]] Vec2 mouseDelta() const noexcept {
        return Vec2{mouse_.x - mousePrev_.x, mouse_.y - mousePrev_.y};
    }
    [[nodiscard]] Vec2 scrollDelta() const noexcept { return scroll_; }

    // --- Gamepad queries -------------------------------------------------

    [[nodiscard]] bool gamepadConnected() const noexcept { return padConnected_; }
    [[nodiscard]] bool gamepadButtonDown(GamepadButton b) const { return padButtons_[index(b)]; }
    [[nodiscard]] bool gamepadButtonPressed(GamepadButton b) const {
        return padButtons_[index(b)] && !padButtonsPrev_[index(b)];
    }
    [[nodiscard]] bool gamepadButtonReleased(GamepadButton b) const {
        return !padButtons_[index(b)] && padButtonsPrev_[index(b)];
    }
    [[nodiscard]] f32 gamepadAxis(GamepadAxis a) const { return padAxes_[index(a)]; }

    // Clear all device state (e.g. on focus loss, to avoid stuck keys).
    void reset() { *this = InputState{}; }

private:
    static constexpr usize index(Key k) noexcept { return static_cast<usize>(k); }
    static constexpr usize index(MouseButton b) noexcept { return static_cast<usize>(b); }
    static constexpr usize index(GamepadButton b) noexcept { return static_cast<usize>(b); }
    static constexpr usize index(GamepadAxis a) noexcept { return static_cast<usize>(a); }

    std::array<bool, kKeyCount> keys_{};
    std::array<bool, kKeyCount> keysPrev_{};
    std::array<bool, kMouseButtonCount> mouseButtons_{};
    std::array<bool, kMouseButtonCount> mouseButtonsPrev_{};
    std::array<bool, kGamepadButtonCount> padButtons_{};
    std::array<bool, kGamepadButtonCount> padButtonsPrev_{};
    std::array<f32, kGamepadAxisCount> padAxes_{};

    Vec2 mouse_{};
    Vec2 mousePrev_{};
    Vec2 scroll_{};
    KeyMods mods_ = KeyMods::None;
    bool padConnected_ = false;
};

}  // namespace zukiru::input

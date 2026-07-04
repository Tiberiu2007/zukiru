// Bridge from platform window events into an InputState.
//
// The platform layer emits backend-neutral WindowEvents with its own input codes;
// this header translates them into the input module's semantic enums and feeds an
// InputState. Typical per-frame use:
//
//   state.beginFrame();
//   window.pollEvents();
//   input::pumpWindowEvents(state, window);
//   if (state.keyPressed(Key::Escape)) quit();
//
// The platform and input enums are defined with identical ordering, so the
// translation is a checked cast (guarded by the static_asserts below) rather than
// a large switch. This is what makes `input` depend on `platform` (Layer 1 → 0).
#pragma once

#include <zukiru/core/types.hpp>
#include <zukiru/input/input_state.hpp>
#include <zukiru/input/key_code.hpp>
#include <zukiru/platform/window.hpp>

namespace zukiru::input {

// platform::Key mirrors input::Key one-to-one; verify a few anchor points so any
// future drift in either enum fails to compile here rather than mis-mapping keys.
static_assert(static_cast<u16>(platform::Key::Unknown) == static_cast<u16>(Key::Unknown));
static_assert(static_cast<u16>(platform::Key::A) == static_cast<u16>(Key::A));
static_assert(static_cast<u16>(platform::Key::Num0) == static_cast<u16>(Key::Num0));
static_assert(static_cast<u16>(platform::Key::Escape) == static_cast<u16>(Key::Escape));
static_assert(static_cast<u16>(platform::Key::LeftSuper) == static_cast<u16>(Key::LeftSuper));
static_assert(static_cast<u16>(platform::Key::Count) == static_cast<u16>(Key::Count));

static_assert(static_cast<u8>(platform::MouseButton::Left) == static_cast<u8>(MouseButton::Left));
static_assert(static_cast<u8>(platform::MouseButton::X2) == static_cast<u8>(MouseButton::X2));
static_assert(static_cast<u8>(platform::MouseButton::Count) == static_cast<u8>(MouseButton::Count));

static_assert(static_cast<u8>(platform::KeyMods::Shift) == static_cast<u8>(KeyMods::Shift));
static_assert(static_cast<u8>(platform::KeyMods::Control) == static_cast<u8>(KeyMods::Control));
static_assert(static_cast<u8>(platform::KeyMods::Alt) == static_cast<u8>(KeyMods::Alt));
static_assert(static_cast<u8>(platform::KeyMods::Super) == static_cast<u8>(KeyMods::Super));

[[nodiscard]] constexpr Key toInputKey(platform::Key key) noexcept {
    return static_cast<Key>(static_cast<u16>(key));
}
[[nodiscard]] constexpr MouseButton toInputMouseButton(platform::MouseButton button) noexcept {
    return static_cast<MouseButton>(static_cast<u8>(button));
}
[[nodiscard]] constexpr KeyMods toInputMods(platform::KeyMods mods) noexcept {
    return static_cast<KeyMods>(static_cast<u8>(mods));
}

// Apply a single window event to `state` (ignores non-input events like resize).
inline void feedEvent(InputState& state, const platform::WindowEvent& event) {
    switch (event.type) {
        case platform::EventType::KeyDown:
            state.onKey(toInputKey(event.key), true, toInputMods(event.mods));
            break;
        case platform::EventType::KeyUp:
            state.onKey(toInputKey(event.key), false, toInputMods(event.mods));
            break;
        case platform::EventType::MouseButtonDown:
            state.onMouseButton(toInputMouseButton(event.button), true);
            break;
        case platform::EventType::MouseButtonUp:
            state.onMouseButton(toInputMouseButton(event.button), false);
            break;
        case platform::EventType::MouseMove:
            state.onMouseMove(event.x, event.y);
            break;
        case platform::EventType::MouseScroll:
            state.onScroll(event.x, event.y);
            break;
        case platform::EventType::WindowClose:
        case platform::EventType::WindowResize:
        case platform::EventType::WindowFocus:
            break;  // not input state
    }
}

// Feed every event the window collected in its last pollEvents() into `state`.
// Call beginFrame() and pollEvents() yourself, in that order, before this.
inline void pumpWindowEvents(InputState& state, const platform::Window& window) {
    for (const platform::WindowEvent& event : window.events()) {
        feedEvent(state, event);
    }
}

}  // namespace zukiru::input

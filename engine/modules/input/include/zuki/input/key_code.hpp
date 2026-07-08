// Device vocabulary: the physical inputs the engine understands.
//
// Keys / buttons / axes are dense, contiguous enums ending in `Count`, so state
// tables can be plain arrays indexed by the enumerator. Values are engine-defined
// (not tied to any OS scancode set); a platform backend translates native codes
// into these before feeding InputState.
#pragma once

#include <zuki/core/types.hpp>

namespace zuki::input {

// Keyboard keys, identified by physical/US-layout meaning. `Unknown` (0) is the
// catch-all for keys a backend can't map. `Count` bounds a per-key array.
enum class Key : u16 {
    Unknown = 0,

    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // Top-row digits
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // Whitespace / editing
    Space, Enter, Tab, Backspace, Delete, Insert, Escape,

    // Navigation
    Left, Right, Up, Down, Home, End, PageUp, PageDown,

    // Symbols (US layout)
    Minus, Equal, LeftBracket, RightBracket, Backslash, Semicolon,
    Apostrophe, Comma, Period, Slash, GraveAccent,

    // Modifiers
    LeftShift, RightShift, LeftControl, RightControl,
    LeftAlt, RightAlt, LeftSuper, RightSuper, CapsLock,

    Count,
};

// Mouse buttons. X1/X2 are the common "back"/"forward" thumb buttons.
enum class MouseButton : u8 {
    Left = 0,
    Right,
    Middle,
    X1,
    X2,
    Count,
};

// Gamepad face/shoulder/dpad buttons, using the Xbox naming as the canonical set
// (a backend maps other pad layouts onto these positions).
enum class GamepadButton : u8 {
    A = 0,
    B,
    X,
    Y,
    LeftBumper,
    RightBumper,
    Back,
    Start,
    Guide,
    LeftThumb,
    RightThumb,
    DpadUp,
    DpadRight,
    DpadDown,
    DpadLeft,
    Count,
};

// Analog gamepad axes. Sticks report [-1, 1]; triggers report [0, 1].
enum class GamepadAxis : u8 {
    LeftX = 0,
    LeftY,
    RightX,
    RightY,
    LeftTrigger,
    RightTrigger,
    Count,
};

// Keyboard modifier state, as a bit flag set. Combine with the operators below.
enum class KeyMods : u8 {
    None = 0,
    Shift = 1 << 0,
    Control = 1 << 1,
    Alt = 1 << 2,
    Super = 1 << 3,
};

[[nodiscard]] constexpr KeyMods operator|(KeyMods a, KeyMods b) noexcept {
    return static_cast<KeyMods>(static_cast<u8>(a) | static_cast<u8>(b));
}
[[nodiscard]] constexpr KeyMods operator&(KeyMods a, KeyMods b) noexcept {
    return static_cast<KeyMods>(static_cast<u8>(a) & static_cast<u8>(b));
}
constexpr KeyMods& operator|=(KeyMods& a, KeyMods b) noexcept { return a = a | b; }
constexpr KeyMods& operator&=(KeyMods& a, KeyMods b) noexcept { return a = a & b; }

// True if every modifier in `required` is present in `have`.
[[nodiscard]] constexpr bool hasMods(KeyMods have, KeyMods required) noexcept {
    return (have & required) == required;
}

// Enumerator counts as plain sizes, for array bounds and loops.
inline constexpr usize kKeyCount = static_cast<usize>(Key::Count);
inline constexpr usize kMouseButtonCount = static_cast<usize>(MouseButton::Count);
inline constexpr usize kGamepadButtonCount = static_cast<usize>(GamepadButton::Count);
inline constexpr usize kGamepadAxisCount = static_cast<usize>(GamepadAxis::Count);

}  // namespace zuki::input

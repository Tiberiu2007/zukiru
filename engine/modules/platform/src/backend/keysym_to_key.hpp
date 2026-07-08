// Shared keysym → platform::Key mapping for the Linux backends.
//
// X11 (XLookupKeysym) and Wayland/xkbcommon (xkb_state_key_get_one_sym) both
// yield X-protocol keysyms — the same numeric values (XK_* == XKB_KEY_*) — so the
// translation lives here once. Values are the fixed X11 keysym codes; names are
// in comments to keep this header free of X11/xkb includes.
#pragma once

#include <zuki/platform/window.hpp>

#include <cstdint>

namespace zuki::platform {

[[nodiscard]] inline Key keysymToKey(std::uint32_t ks) noexcept {
    const auto offset = [](Key base, std::uint32_t delta) {
        return static_cast<Key>(static_cast<u16>(base) + static_cast<u16>(delta));
    };

    if (ks >= 0x61 && ks <= 0x7a) return offset(Key::A, ks - 0x61);      // a..z
    if (ks >= 0x41 && ks <= 0x5a) return offset(Key::A, ks - 0x41);      // A..Z
    if (ks >= 0x30 && ks <= 0x39) return offset(Key::Num0, ks - 0x30);   // 0..9
    if (ks >= 0xffbe && ks <= 0xffc9) return offset(Key::F1, ks - 0xffbe);  // F1..F12

    switch (ks) {
        case 0x20: return Key::Space;
        case 0xff0d: return Key::Enter;       // Return
        case 0xff09: return Key::Tab;
        case 0xff08: return Key::Backspace;   // BackSpace
        case 0xffff: return Key::Delete;
        case 0xff63: return Key::Insert;
        case 0xff1b: return Key::Escape;
        case 0xff51: return Key::Left;
        case 0xff53: return Key::Right;
        case 0xff52: return Key::Up;
        case 0xff54: return Key::Down;
        case 0xff50: return Key::Home;
        case 0xff57: return Key::End;
        case 0xff55: return Key::PageUp;      // Page_Up
        case 0xff56: return Key::PageDown;    // Page_Down
        case 0x2d: return Key::Minus;
        case 0x3d: return Key::Equal;
        case 0x5b: return Key::LeftBracket;   // bracketleft
        case 0x5d: return Key::RightBracket;  // bracketright
        case 0x5c: return Key::Backslash;
        case 0x3b: return Key::Semicolon;
        case 0x27: return Key::Apostrophe;
        case 0x2c: return Key::Comma;
        case 0x2e: return Key::Period;
        case 0x2f: return Key::Slash;
        case 0x60: return Key::GraveAccent;   // grave
        case 0xffe1: return Key::LeftShift;   // Shift_L
        case 0xffe2: return Key::RightShift;  // Shift_R
        case 0xffe3: return Key::LeftControl;
        case 0xffe4: return Key::RightControl;
        case 0xffe9: return Key::LeftAlt;     // Alt_L
        case 0xffea: return Key::RightAlt;    // Alt_R
        case 0xffeb: return Key::LeftSuper;   // Super_L
        case 0xffec: return Key::RightSuper;  // Super_R
        case 0xffe5: return Key::CapsLock;    // Caps_Lock
        default: return Key::Unknown;
    }
}

}  // namespace zuki::platform

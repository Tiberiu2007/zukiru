// Window — the platform windowing surface and its input/event stream.
//
// A concrete backend is selected at runtime by createWindow() (Wayland if
// WAYLAND_DISPLAY is set, else X11 if DISPLAY is set). Windows (Win32) and macOS
// (Cocoa) backends are future work — see docs/adr/0005-native-windowing-backends.md.
//
//   auto win = platform::createWindow({.title = "Zukiru", .width = 1280, .height = 720});
//   while (!win.value()->shouldClose()) {
//       win.value()->pollEvents();
//       for (const platform::WindowEvent& e : win.value()->events()) { ... }
//   }
//
// The input codes here are platform-level; the `input` module maps them to its
// own semantic enums via input/platform_bridge.hpp.
#pragma once

#include <zukiru/core/result.hpp>
#include <zukiru/core/types.hpp>

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace zukiru::platform {

// --- Input vocabulary -----------------------------------------------------
// Key enumerators mirror input::Key one-to-one (same order) so the input bridge
// can translate with a checked cast. Physical/US-layout meaning; Unknown for keys
// a backend can't map.
enum class Key : u16 {
    Unknown = 0,

    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    Space, Enter, Tab, Backspace, Delete, Insert, Escape,

    Left, Right, Up, Down, Home, End, PageUp, PageDown,

    Minus, Equal, LeftBracket, RightBracket, Backslash, Semicolon,
    Apostrophe, Comma, Period, Slash, GraveAccent,

    LeftShift, RightShift, LeftControl, RightControl,
    LeftAlt, RightAlt, LeftSuper, RightSuper, CapsLock,

    Count,
};

enum class MouseButton : u8 {
    Left = 0,
    Right,
    Middle,
    X1,
    X2,
    Count,
};

// Modifier bit flags (mirrors input::KeyMods).
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

// --- Events ---------------------------------------------------------------

enum class EventType : u8 {
    WindowClose,        // user/OS asked the window to close
    WindowResize,       // framebuffer/window size changed (width, height)
    WindowFocus,        // gained/lost focus (focused)
    KeyDown,            // key, mods
    KeyUp,              // key, mods
    MouseButtonDown,    // button, x, y
    MouseButtonUp,      // button, x, y
    MouseMove,          // x, y (absolute, window pixels)
    MouseScroll,        // x, y (scroll offset; y is the common vertical wheel)
};

// A single input/window event. Fields are populated per `type` (see EventType).
struct WindowEvent {
    EventType type{};
    Key key = Key::Unknown;
    MouseButton button = MouseButton::Left;
    KeyMods mods = KeyMods::None;
    f32 x = 0.0f;
    f32 y = 0.0f;
    u32 width = 0;
    u32 height = 0;
    bool focused = false;
};

// --- Window ---------------------------------------------------------------

enum class WindowMode : u8 {
    Windowed,
    Fullscreen,
    BorderlessFullscreen,
};

struct WindowExtent {
    u32 width = 0;
    u32 height = 0;

    friend constexpr bool operator==(WindowExtent, WindowExtent) = default;
};

// Creation parameters. Defaults describe a resizable 1280x720 windowed surface.
struct WindowConfig {
    std::string title = "Zukiru";
    u32 width = 1280;
    u32 height = 720;
    WindowMode mode = WindowMode::Windowed;
    bool resizable = true;
    bool visible = true;
};

// A platform window. Backends fill `events_` during pollEvents(); the render
// backend obtains a surface from nativeHandle()/nativeDisplay().
class Window {
public:
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    virtual ~Window() = default;

    // Pump the OS event queue; refills events() with what arrived since the last
    // call. Call once per frame.
    virtual void pollEvents() = 0;

    // Events collected by the most recent pollEvents().
    [[nodiscard]] std::span<const WindowEvent> events() const noexcept { return events_; }

    // True once the user/OS has requested the window close.
    [[nodiscard]] virtual bool shouldClose() const = 0;
    virtual void setShouldClose(bool value) = 0;

    [[nodiscard]] virtual WindowExtent extent() const = 0;
    virtual void setTitle(std::string_view title) = 0;

    // Opaque native handles for the render backend. For X11: nativeHandle() is the
    // X11 Window (as uintptr in a void*), nativeDisplay() the Display*. For
    // Wayland: nativeHandle() is the wl_surface*, nativeDisplay() the wl_display*.
    [[nodiscard]] virtual void* nativeHandle() const = 0;
    [[nodiscard]] virtual void* nativeDisplay() const = 0;

protected:
    Window() = default;
    std::vector<WindowEvent> events_;
};

// Create a platform window using the runtime-selected backend. Returns an Error
// if no display is available or no backend was compiled in.
[[nodiscard]] Result<std::unique_ptr<Window>> createWindow(const WindowConfig& config);

// Which windowing backends were compiled into this build.
struct WindowBackends {
    bool x11 = false;
    bool wayland = false;
};
[[nodiscard]] WindowBackends compiledWindowBackends() noexcept;

}  // namespace zukiru::platform

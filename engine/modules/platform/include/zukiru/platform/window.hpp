// Window — the abstract windowing surface the renderer and app loop target.
//
// This header defines the *interface* only. A concrete backend (GLFW / SDL /
// native Win32 / X11 / Cocoa) is deferred until the windowing dependency is
// chosen and a display is available — see docs/adr/0003-windowing-backend.md.
// `createWindow()` therefore currently returns an error; downstream code can
// still compile and link against this API today.
#pragma once

#include <zukiru/core/result.hpp>
#include <zukiru/core/types.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace zukiru::platform {

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

// A platform window. Concrete backends implement this; the render module obtains
// a surface from nativeHandle().
class Window {
public:
    Window() = default;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    virtual ~Window() = default;

    // Pump the OS event queue; call once per frame.
    virtual void pollEvents() = 0;

    // True once the user/OS has requested the window close.
    [[nodiscard]] virtual bool shouldClose() const = 0;

    [[nodiscard]] virtual WindowExtent extent() const = 0;
    virtual void setTitle(std::string_view title) = 0;

    // Opaque native handle for the render backend (e.g. HWND, Wayland surface,
    // NSWindow*). Interpretation is backend-specific.
    [[nodiscard]] virtual void* nativeHandle() const = 0;
};

// Create a platform window. Currently returns an error until a windowing backend
// is configured (see the file header and ADR 0003).
[[nodiscard]] Result<std::unique_ptr<Window>> createWindow(const WindowConfig& config);

}  // namespace zukiru::platform

#include <zukiru/platform/window.hpp>

#include <cstdlib>

#if ZUKIRU_WINDOW_WAYLAND
#include "backend/wayland/wayland_window.hpp"
#endif
#if ZUKIRU_WINDOW_X11
#include "backend/x11/x11_window.hpp"
#endif

namespace zukiru::platform {
namespace {

[[maybe_unused]] [[nodiscard]] bool envSet(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0';
}

}  // namespace

WindowBackends compiledWindowBackends() noexcept {
#if ZUKIRU_WINDOW_X11
    constexpr bool kX11 = true;
#else
    constexpr bool kX11 = false;
#endif
#if ZUKIRU_WINDOW_WAYLAND
    constexpr bool kWayland = true;
#else
    constexpr bool kWayland = false;
#endif
    return WindowBackends{kX11, kWayland};
}

Result<std::unique_ptr<Window>> createWindow(const WindowConfig& config) {
    // Prefer Wayland when running under a Wayland session; fall back to X11 (which
    // also covers XWayland). A backend compiled out is simply skipped.
#if ZUKIRU_WINDOW_WAYLAND
    if (envSet("WAYLAND_DISPLAY")) {
        Result<std::unique_ptr<Window>> wayland = createWaylandWindow(config);
        if (wayland.isOk()) return wayland;
        // Wayland connection failed — fall through to X11 if it is available.
    }
#endif
#if ZUKIRU_WINDOW_X11
    if (envSet("DISPLAY")) {
        return createX11Window(config);
    }
#endif
    (void)config;
    return Err(Error{"createWindow: no windowing backend available "
                     "(no WAYLAND_DISPLAY/DISPLAY, or backends not compiled in)"});
}

}  // namespace zukiru::platform

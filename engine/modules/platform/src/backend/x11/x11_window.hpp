// X11 (Xlib) windowing backend factory. Private to the platform module.
#pragma once

#include <zukiru/platform/window.hpp>

#include <memory>

namespace zukiru::platform {

// Create an Xlib-backed window. Returns an Error if the X display can't be opened
// or the window can't be created.
[[nodiscard]] Result<std::unique_ptr<Window>> createX11Window(const WindowConfig& config);

}  // namespace zukiru::platform

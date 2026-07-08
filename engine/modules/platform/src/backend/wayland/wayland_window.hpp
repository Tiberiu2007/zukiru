// Wayland (libwayland-client + xdg-shell + xkbcommon) windowing backend factory.
// Private to the platform module.
#pragma once

#include <zuki/platform/window.hpp>

#include <memory>

namespace zuki::platform {

// Create a Wayland-backed window. Returns an Error if the compositor can't be
// reached or a required global (compositor / xdg_wm_base / shm) is missing.
[[nodiscard]] Result<std::unique_ptr<Window>> createWaylandWindow(const WindowConfig& config);

}  // namespace zuki::platform

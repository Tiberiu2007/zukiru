#include <zukiru/platform/window.hpp>

namespace zukiru::platform {

Result<std::unique_ptr<Window>> createWindow(const WindowConfig& /*config*/) {
    // No concrete windowing backend is wired up yet. Choosing one (GLFW / SDL /
    // native) is tracked in docs/adr/0003-windowing-backend.md and agents/TODO.md.
    return Err(Error{"platform: no windowing backend configured (see ADR 0003)"});
}

}  // namespace zukiru::platform

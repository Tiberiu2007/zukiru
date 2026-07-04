// Wayland windowing backend.
//
// Binds the minimum set of globals (wl_compositor, xdg_wm_base, wl_shm, wl_seat)
// at version 1 to keep the listener surface small and avoid null-handler crashes
// from newer optional events. An xdg-shell toplevel is created and made visible
// with a wl_shm buffer (no GPU renderer exists yet); keyboard is decoded through
// xkbcommon. See docs/adr/0005-native-windowing-backends.md.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE  // memfd_create
#endif
#include "backend/wayland/wayland_window.hpp"

#include "backend/keysym_to_key.hpp"

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "xdg-shell-client-protocol.h"

#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>
#include <string>

namespace zukiru::platform {
namespace {

// Create an anonymous, memory-backed file of `size` bytes for a wl_shm pool.
[[nodiscard]] int createShmFile(usize size) {
    int fd = memfd_create("zukiru-wl-shm", MFD_CLOEXEC);
    if (fd < 0) return -1;
    if (ftruncate(fd, static_cast<off_t>(size)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

[[nodiscard]] MouseButton translateButton(uint32_t code, bool& known) {
    known = true;
    switch (code) {
        case BTN_LEFT: return MouseButton::Left;
        case BTN_RIGHT: return MouseButton::Right;
        case BTN_MIDDLE: return MouseButton::Middle;
        case BTN_SIDE: return MouseButton::X1;
        case BTN_EXTRA: return MouseButton::X2;
        default: known = false; return MouseButton::Left;
    }
}

class WaylandWindow final : public Window {
public:
    WaylandWindow() = default;
    ~WaylandWindow() override { destroy(); }

    // Connect and set everything up. Returns an error string on failure.
    [[nodiscard]] const char* init(const WindowConfig& config) {
        title_ = config.title;
        width_ = config.width;
        height_ = config.height;

        display_ = wl_display_connect(nullptr);
        if (display_ == nullptr) return "Wayland: wl_display_connect failed";

        registry_ = wl_display_get_registry(display_);
        wl_registry_add_listener(registry_, &kRegistryListener, this);
        wl_display_roundtrip(display_);  // receive globals

        if (compositor_ == nullptr || wmBase_ == nullptr || shm_ == nullptr) {
            return "Wayland: missing required globals (compositor/xdg_wm_base/shm)";
        }

        surface_ = wl_compositor_create_surface(compositor_);
        xdgSurface_ = xdg_wm_base_get_xdg_surface(wmBase_, surface_);
        xdg_surface_add_listener(xdgSurface_, &kXdgSurfaceListener, this);
        xdgToplevel_ = xdg_surface_get_toplevel(xdgSurface_);
        xdg_toplevel_add_listener(xdgToplevel_, &kXdgToplevelListener, this);
        xdg_toplevel_set_title(xdgToplevel_, title_.c_str());
        xdg_toplevel_set_app_id(xdgToplevel_, "zukiru");

        wl_surface_commit(surface_);   // initial commit → triggers first configure
        wl_display_roundtrip(display_);  // process configure (attaches the buffer)
        return nullptr;
    }

    void pollEvents() override {
        events_.clear();
        // Non-blocking dispatch of whatever has arrived.
        while (wl_display_prepare_read(display_) != 0) {
            wl_display_dispatch_pending(display_);
        }
        wl_display_flush(display_);

        pollfd pfd{wl_display_get_fd(display_), POLLIN, 0};
        if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN) != 0) {
            wl_display_read_events(display_);
        } else {
            wl_display_cancel_read(display_);
        }
        wl_display_dispatch_pending(display_);
    }

    [[nodiscard]] bool shouldClose() const override { return shouldClose_; }
    void setShouldClose(bool value) override { shouldClose_ = value; }

    [[nodiscard]] WindowExtent extent() const override { return {width_, height_}; }

    void setTitle(std::string_view title) override {
        title_.assign(title);
        if (xdgToplevel_ != nullptr) xdg_toplevel_set_title(xdgToplevel_, title_.c_str());
    }

    [[nodiscard]] void* nativeHandle() const override { return surface_; }
    [[nodiscard]] void* nativeDisplay() const override { return display_; }

private:
    void push(const WindowEvent& event) { events_.push_back(event); }

    // --- Buffer management ----------------------------------------------
    void recreateBuffer() {
        const usize stride = static_cast<usize>(width_) * 4;
        const usize size = stride * height_;
        if (size == 0) return;

        const int fd = createShmFile(size);
        if (fd < 0) return;
        void* mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (mapped == MAP_FAILED) {
            close(fd);
            return;
        }
        auto* data = static_cast<u32*>(mapped);
        // Fill with an opaque dark gray so the surface is visible pre-renderer.
        for (usize i = 0; i < size / 4; ++i) data[i] = 0xff303030u;

        wl_shm_pool* pool = wl_shm_create_pool(shm_, fd, static_cast<int32_t>(size));
        wl_buffer* buffer = wl_shm_pool_create_buffer(
            pool, 0, static_cast<int32_t>(width_), static_cast<int32_t>(height_),
            static_cast<int32_t>(stride), WL_SHM_FORMAT_XRGB8888);
        wl_shm_pool_destroy(pool);
        close(fd);

        if (bufferData_ != nullptr) munmap(bufferData_, bufferSize_);
        if (buffer_ != nullptr) wl_buffer_destroy(buffer_);
        bufferData_ = data;
        bufferSize_ = size;
        buffer_ = buffer;

        wl_surface_attach(surface_, buffer_, 0, 0);
        wl_surface_damage(surface_, 0, 0, static_cast<int32_t>(width_),
                          static_cast<int32_t>(height_));
    }

    void destroy() {
        if (keyboard_ != nullptr) wl_keyboard_destroy(keyboard_);
        if (pointer_ != nullptr) wl_pointer_destroy(pointer_);
        if (seat_ != nullptr) wl_seat_destroy(seat_);
        if (xkbState_ != nullptr) xkb_state_unref(xkbState_);
        if (xkbKeymap_ != nullptr) xkb_keymap_unref(xkbKeymap_);
        if (xkbContext_ != nullptr) xkb_context_unref(xkbContext_);
        if (buffer_ != nullptr) wl_buffer_destroy(buffer_);
        if (bufferData_ != nullptr) munmap(bufferData_, bufferSize_);
        if (xdgToplevel_ != nullptr) xdg_toplevel_destroy(xdgToplevel_);
        if (xdgSurface_ != nullptr) xdg_surface_destroy(xdgSurface_);
        if (surface_ != nullptr) wl_surface_destroy(surface_);
        if (wmBase_ != nullptr) xdg_wm_base_destroy(wmBase_);
        if (shm_ != nullptr) wl_shm_destroy(shm_);
        if (compositor_ != nullptr) wl_compositor_destroy(compositor_);
        if (registry_ != nullptr) wl_registry_destroy(registry_);
        if (display_ != nullptr) wl_display_disconnect(display_);
    }

    // --- Registry --------------------------------------------------------
    void onGlobal(wl_registry* registry, uint32_t name, const char* interface, uint32_t /*ver*/) {
        if (std::strcmp(interface, wl_compositor_interface.name) == 0) {
            compositor_ = static_cast<wl_compositor*>(
                wl_registry_bind(registry, name, &wl_compositor_interface, 1));
        } else if (std::strcmp(interface, xdg_wm_base_interface.name) == 0) {
            wmBase_ = static_cast<xdg_wm_base*>(
                wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
            xdg_wm_base_add_listener(wmBase_, &kWmBaseListener, this);
        } else if (std::strcmp(interface, wl_shm_interface.name) == 0) {
            shm_ = static_cast<wl_shm*>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
        } else if (std::strcmp(interface, wl_seat_interface.name) == 0) {
            seat_ = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 1));
            wl_seat_add_listener(seat_, &kSeatListener, this);
        }
    }

    // --- Seat / input ----------------------------------------------------
    void onSeatCapabilities(wl_seat* seat, uint32_t caps) {
        const bool hasKeyboard = (caps & WL_SEAT_CAPABILITY_KEYBOARD) != 0;
        if (hasKeyboard && keyboard_ == nullptr) {
            keyboard_ = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(keyboard_, &kKeyboardListener, this);
        }
        const bool hasPointer = (caps & WL_SEAT_CAPABILITY_POINTER) != 0;
        if (hasPointer && pointer_ == nullptr) {
            pointer_ = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(pointer_, &kPointerListener, this);
        }
    }

    void onKeymap(uint32_t format, int32_t fd, uint32_t size) {
        if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
            close(fd);
            return;
        }
        char* map = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
        if (map == MAP_FAILED) {
            close(fd);
            return;
        }
        if (xkbContext_ == nullptr) xkbContext_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        xkb_keymap* keymap = xkb_keymap_new_from_string(
            xkbContext_, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        munmap(map, size);
        close(fd);
        if (keymap == nullptr) return;

        if (xkbKeymap_ != nullptr) xkb_keymap_unref(xkbKeymap_);
        if (xkbState_ != nullptr) xkb_state_unref(xkbState_);
        xkbKeymap_ = keymap;
        xkbState_ = xkb_state_new(xkbKeymap_);
    }

    void onKey(uint32_t key, uint32_t state) {
        if (xkbState_ == nullptr) return;
        const xkb_keycode_t code = key + 8;  // evdev → xkb keycode offset
        const xkb_keysym_t sym = xkb_state_key_get_one_sym(xkbState_, code);

        WindowEvent e;
        e.type = state == WL_KEYBOARD_KEY_STATE_PRESSED ? EventType::KeyDown : EventType::KeyUp;
        e.key = keysymToKey(static_cast<uint32_t>(sym));
        e.mods = mods_;
        push(e);
    }

    void onModifiers(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
        if (xkbState_ == nullptr) return;
        xkb_state_update_mask(xkbState_, depressed, latched, locked, 0, 0, group);
        mods_ = KeyMods::None;
        if (xkb_state_mod_name_is_active(xkbState_, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0)
            mods_ |= KeyMods::Shift;
        if (xkb_state_mod_name_is_active(xkbState_, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0)
            mods_ |= KeyMods::Control;
        if (xkb_state_mod_name_is_active(xkbState_, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0)
            mods_ |= KeyMods::Alt;
        if (xkb_state_mod_name_is_active(xkbState_, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0)
            mods_ |= KeyMods::Super;
    }

    void onPointerMotion(wl_fixed_t x, wl_fixed_t y) {
        cursorX_ = static_cast<f32>(wl_fixed_to_double(x));
        cursorY_ = static_cast<f32>(wl_fixed_to_double(y));
        WindowEvent e;
        e.type = EventType::MouseMove;
        e.x = cursorX_;
        e.y = cursorY_;
        push(e);
    }

    void onPointerButton(uint32_t button, uint32_t state) {
        bool known = false;
        const MouseButton mapped = translateButton(button, known);
        if (!known) return;
        WindowEvent e;
        e.type = state == WL_POINTER_BUTTON_STATE_PRESSED ? EventType::MouseButtonDown
                                                          : EventType::MouseButtonUp;
        e.button = mapped;
        e.x = cursorX_;
        e.y = cursorY_;
        push(e);
    }

    void onPointerAxis(uint32_t axis, wl_fixed_t value) {
        const double v = wl_fixed_to_double(value);
        WindowEvent e;
        e.type = EventType::MouseScroll;
        // Wayland: positive = down / right. Engine: up / right = positive.
        if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
            e.y = v > 0.0 ? -1.0f : 1.0f;
        } else {
            e.x = v > 0.0 ? 1.0f : -1.0f;
        }
        push(e);
    }

    // --- xdg-shell -------------------------------------------------------
    void onXdgSurfaceConfigure(xdg_surface* xdgSurface, uint32_t serial) {
        xdg_surface_ack_configure(xdgSurface, serial);
        if (!configured_) {
            configured_ = true;
            recreateBuffer();
        }
        wl_surface_commit(surface_);
    }

    void onToplevelConfigure(int32_t width, int32_t height) {
        if (width <= 0 || height <= 0) return;  // compositor deferring to us
        const auto w = static_cast<u32>(width);
        const auto h = static_cast<u32>(height);
        if (w == width_ && h == height_) return;
        width_ = w;
        height_ = h;
        if (configured_) recreateBuffer();
        WindowEvent e;
        e.type = EventType::WindowResize;
        e.width = w;
        e.height = h;
        push(e);
    }

    void onToplevelClose() {
        shouldClose_ = true;
        WindowEvent e;
        e.type = EventType::WindowClose;
        push(e);
    }

    // --- Static listener trampolines (data == WaylandWindow*) ------------
    static WaylandWindow& self(void* data) { return *static_cast<WaylandWindow*>(data); }

    static const wl_registry_listener kRegistryListener;
    static const wl_seat_listener kSeatListener;
    static const wl_keyboard_listener kKeyboardListener;
    static const wl_pointer_listener kPointerListener;
    static const xdg_wm_base_listener kWmBaseListener;
    static const xdg_surface_listener kXdgSurfaceListener;
    static const xdg_toplevel_listener kXdgToplevelListener;

    // Wayland objects.
    wl_display* display_ = nullptr;
    wl_registry* registry_ = nullptr;
    wl_compositor* compositor_ = nullptr;
    wl_shm* shm_ = nullptr;
    xdg_wm_base* wmBase_ = nullptr;
    wl_seat* seat_ = nullptr;
    wl_keyboard* keyboard_ = nullptr;
    wl_pointer* pointer_ = nullptr;
    wl_surface* surface_ = nullptr;
    xdg_surface* xdgSurface_ = nullptr;
    xdg_toplevel* xdgToplevel_ = nullptr;
    wl_buffer* buffer_ = nullptr;

    // xkbcommon keyboard state.
    xkb_context* xkbContext_ = nullptr;
    xkb_keymap* xkbKeymap_ = nullptr;
    xkb_state* xkbState_ = nullptr;

    void* bufferData_ = nullptr;
    usize bufferSize_ = 0;

    std::string title_;
    u32 width_ = 0;
    u32 height_ = 0;
    f32 cursorX_ = 0.0f;
    f32 cursorY_ = 0.0f;
    KeyMods mods_ = KeyMods::None;
    bool configured_ = false;
    bool shouldClose_ = false;
};

// --- Listener tables ------------------------------------------------------
// Newer wayland-client headers add trailing listener members (e.g. wl_pointer
// axis_value120); we only handle what our bound (v1) objects can emit and let the
// rest zero-initialize. Silence the resulting missing-initializer diagnostic.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

const wl_registry_listener WaylandWindow::kRegistryListener{
    .global =
        [](void* data, wl_registry* reg, uint32_t name, const char* iface, uint32_t ver) {
            self(data).onGlobal(reg, name, iface, ver);
        },
    .global_remove = [](void*, wl_registry*, uint32_t) {},
};

const wl_seat_listener WaylandWindow::kSeatListener{
    .capabilities =
        [](void* data, wl_seat* seat, uint32_t caps) { self(data).onSeatCapabilities(seat, caps); },
    .name = [](void*, wl_seat*, const char*) {},
};

const wl_keyboard_listener WaylandWindow::kKeyboardListener{
    .keymap = [](void* data, wl_keyboard*, uint32_t fmt, int32_t fd,
                 uint32_t size) { self(data).onKeymap(fmt, fd, size); },
    .enter = [](void*, wl_keyboard*, uint32_t, wl_surface*, wl_array*) {},
    .leave = [](void*, wl_keyboard*, uint32_t, wl_surface*) {},
    .key = [](void* data, wl_keyboard*, uint32_t, uint32_t, uint32_t key,
              uint32_t state) { self(data).onKey(key, state); },
    .modifiers = [](void* data, wl_keyboard*, uint32_t, uint32_t depressed, uint32_t latched,
                    uint32_t locked,
                    uint32_t group) { self(data).onModifiers(depressed, latched, locked, group); },
    .repeat_info = [](void*, wl_keyboard*, int32_t, int32_t) {},
};

const wl_pointer_listener WaylandWindow::kPointerListener{
    .enter = [](void*, wl_pointer*, uint32_t, wl_surface*, wl_fixed_t, wl_fixed_t) {},
    .leave = [](void*, wl_pointer*, uint32_t, wl_surface*) {},
    .motion = [](void* data, wl_pointer*, uint32_t, wl_fixed_t x,
                 wl_fixed_t y) { self(data).onPointerMotion(x, y); },
    .button = [](void* data, wl_pointer*, uint32_t, uint32_t, uint32_t button,
                 uint32_t state) { self(data).onPointerButton(button, state); },
    .axis = [](void* data, wl_pointer*, uint32_t, uint32_t axis,
               wl_fixed_t value) { self(data).onPointerAxis(axis, value); },
    .frame = [](void*, wl_pointer*) {},
    .axis_source = [](void*, wl_pointer*, uint32_t) {},
    .axis_stop = [](void*, wl_pointer*, uint32_t, uint32_t) {},
    .axis_discrete = [](void*, wl_pointer*, uint32_t, int32_t) {},
};

const xdg_wm_base_listener WaylandWindow::kWmBaseListener{
    .ping = [](void*, xdg_wm_base* base, uint32_t serial) { xdg_wm_base_pong(base, serial); },
};

const xdg_surface_listener WaylandWindow::kXdgSurfaceListener{
    .configure = [](void* data, xdg_surface* surf,
                    uint32_t serial) { self(data).onXdgSurfaceConfigure(surf, serial); },
};

const xdg_toplevel_listener WaylandWindow::kXdgToplevelListener{
    .configure = [](void* data, xdg_toplevel*, int32_t width, int32_t height,
                    wl_array*) { self(data).onToplevelConfigure(width, height); },
    .close = [](void* data, xdg_toplevel*) { self(data).onToplevelClose(); },
};

#pragma GCC diagnostic pop

}  // namespace

Result<std::unique_ptr<Window>> createWaylandWindow(const WindowConfig& config) {
    auto window = std::make_unique<WaylandWindow>();
    if (const char* error = window->init(config); error != nullptr) {
        return Err(Error{error});
    }
    return Ok(std::unique_ptr<Window>{std::move(window)});
}

}  // namespace zukiru::platform

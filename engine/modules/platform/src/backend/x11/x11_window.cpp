#include "backend/x11/x11_window.hpp"

#include "backend/keysym_to_key.hpp"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

// Xlib pollutes the global namespace with lowercase macros; drop the ones that
// collide with our identifiers before any C++ code below.
#ifdef None
#undef None
#endif

#include <cstdint>

namespace zuki::platform {
namespace {

using XWindowHandle = ::Window;  // Xlib's Window (an XID); distinct from our Window class

[[nodiscard]] KeyMods translateMods(unsigned int state) noexcept {
    KeyMods mods = KeyMods::None;
    if ((state & ShiftMask) != 0u) mods |= KeyMods::Shift;
    if ((state & ControlMask) != 0u) mods |= KeyMods::Control;
    if ((state & Mod1Mask) != 0u) mods |= KeyMods::Alt;     // Mod1 == Alt
    if ((state & Mod4Mask) != 0u) mods |= KeyMods::Super;   // Mod4 == Super/Win
    return mods;
}

class X11Window final : public Window {
public:
    X11Window(Display* display, XWindowHandle window, Atom wmDeleteWindow, u32 width, u32 height)
        : display_(display), window_(window), wmDeleteWindow_(wmDeleteWindow), width_(width),
          height_(height) {}

    ~X11Window() override {
        XDestroyWindow(display_, window_);
        XCloseDisplay(display_);
    }

    void pollEvents() override {
        events_.clear();
        XEvent event;
        while (XPending(display_) > 0) {
            XNextEvent(display_, &event);
            dispatch(event);
        }
    }

    [[nodiscard]] bool shouldClose() const override { return shouldClose_; }
    void setShouldClose(bool value) override { shouldClose_ = value; }

    [[nodiscard]] WindowExtent extent() const override { return {width_, height_}; }

    void setTitle(std::string_view title) override {
        const std::string owned{title};
        XStoreName(display_, window_, owned.c_str());
        XFlush(display_);
    }

    [[nodiscard]] void* nativeHandle() const override {
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(window_));
    }
    [[nodiscard]] void* nativeDisplay() const override { return static_cast<void*>(display_); }
    [[nodiscard]] NativeBackend nativeBackend() const override { return NativeBackend::X11; }

private:
    void push(const WindowEvent& event) { events_.push_back(event); }

    void dispatch(XEvent& event) {
        switch (event.type) {
            case ConfigureNotify: {
                const auto w = static_cast<u32>(event.xconfigure.width);
                const auto h = static_cast<u32>(event.xconfigure.height);
                if (w != width_ || h != height_) {
                    width_ = w;
                    height_ = h;
                    WindowEvent e;
                    e.type = EventType::WindowResize;
                    e.width = w;
                    e.height = h;
                    push(e);
                }
                break;
            }
            case ClientMessage: {
                if (static_cast<Atom>(event.xclient.data.l[0]) == wmDeleteWindow_) {
                    shouldClose_ = true;
                    WindowEvent e;
                    e.type = EventType::WindowClose;
                    push(e);
                }
                break;
            }
            case KeyPress: {
                WindowEvent e;
                e.type = EventType::KeyDown;
                e.key = keysymToKey(static_cast<std::uint32_t>(XLookupKeysym(&event.xkey, 0)));
                e.mods = translateMods(event.xkey.state);
                push(e);
                break;
            }
            case KeyRelease: {
                // Xlib delivers auto-repeat as a Release immediately followed by a
                // Press with the same time/keycode. Collapse it: the key stays
                // down, so emit nothing.
                if (XPending(display_) > 0) {
                    XEvent next;
                    XPeekEvent(display_, &next);
                    if (next.type == KeyPress && next.xkey.time == event.xkey.time &&
                        next.xkey.keycode == event.xkey.keycode) {
                        XNextEvent(display_, &next);  // drop the repeat Press too
                        break;
                    }
                }
                WindowEvent e;
                e.type = EventType::KeyUp;
                e.key = keysymToKey(static_cast<std::uint32_t>(XLookupKeysym(&event.xkey, 0)));
                e.mods = translateMods(event.xkey.state);
                push(e);
                break;
            }
            case ButtonPress:
                dispatchButton(event.xbutton, /*down=*/true);
                break;
            case ButtonRelease:
                dispatchButton(event.xbutton, /*down=*/false);
                break;
            case MotionNotify: {
                WindowEvent e;
                e.type = EventType::MouseMove;
                e.x = static_cast<f32>(event.xmotion.x);
                e.y = static_cast<f32>(event.xmotion.y);
                push(e);
                break;
            }
            case FocusIn:
            case FocusOut: {
                WindowEvent e;
                e.type = EventType::WindowFocus;
                e.focused = event.type == FocusIn;
                push(e);
                break;
            }
            default:
                break;
        }
    }

    void dispatchButton(const XButtonEvent& button, bool down) {
        // Buttons 4/5 (and 6/7) are the scroll wheel; they only fire on press.
        if (button.button == 4 || button.button == 5) {
            if (!down) return;
            WindowEvent e;
            e.type = EventType::MouseScroll;
            e.y = button.button == 4 ? 1.0f : -1.0f;
            push(e);
            return;
        }
        if (button.button == 6 || button.button == 7) {
            if (!down) return;
            WindowEvent e;
            e.type = EventType::MouseScroll;
            e.x = button.button == 6 ? -1.0f : 1.0f;
            push(e);
            return;
        }

        WindowEvent e;
        e.type = down ? EventType::MouseButtonDown : EventType::MouseButtonUp;
        e.x = static_cast<f32>(button.x);
        e.y = static_cast<f32>(button.y);
        switch (button.button) {
            case 1: e.button = MouseButton::Left; break;
            case 2: e.button = MouseButton::Middle; break;
            case 3: e.button = MouseButton::Right; break;
            case 8: e.button = MouseButton::X1; break;
            case 9: e.button = MouseButton::X2; break;
            default: return;  // unknown button
        }
        push(e);
    }

    Display* display_;
    XWindowHandle window_;
    Atom wmDeleteWindow_;
    u32 width_;
    u32 height_;
    bool shouldClose_ = false;
};

}  // namespace

Result<std::unique_ptr<Window>> createX11Window(const WindowConfig& config) {
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        return Err(Error{"X11: XOpenDisplay failed (no reachable X server)"});
    }

    const int screen = DefaultScreen(display);
    const XWindowHandle root = RootWindow(display, screen);
    const XWindowHandle window = XCreateSimpleWindow(
        display, root, 0, 0, config.width, config.height, 1, BlackPixel(display, screen),
        WhitePixel(display, screen));
    if (window == 0) {
        XCloseDisplay(display);
        return Err(Error{"X11: XCreateSimpleWindow failed"});
    }

    XStoreName(display, window, config.title.c_str());

    // Fix the size for non-resizable windows via WM size hints.
    if (!config.resizable) {
        if (XSizeHints* hints = XAllocSizeHints(); hints != nullptr) {
            hints->flags = PMinSize | PMaxSize;
            hints->min_width = hints->max_width = static_cast<int>(config.width);
            hints->min_height = hints->max_height = static_cast<int>(config.height);
            XSetWMNormalHints(display, window, hints);
            XFree(hints);
        }
    }

    Atom wmDeleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wmDeleteWindow, 1);

    XSelectInput(display, window,
                 ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                     ButtonReleaseMask | PointerMotionMask | StructureNotifyMask |
                     FocusChangeMask);

    if (config.visible) {
        XMapWindow(display, window);
    }
    XFlush(display);

    return Ok(std::unique_ptr<Window>{
        std::make_unique<X11Window>(display, window, wmDeleteWindow, config.width, config.height)});
}

}  // namespace zuki::platform

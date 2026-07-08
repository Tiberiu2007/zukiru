# input — Layer 1 (services). Device abstraction (keyboard/mouse/gamepad state
# with per-frame edge detection) and rebindable action mapping (named button and
# axis actions over physical inputs). Header-only. The optional platform bridge
# (input/platform_bridge.hpp) translates platform window events into an
# InputState, which is why platform is a (public) dependency. Namespace zuki::input.
add_zuki_module(input PUBLIC_DEPS core platform)

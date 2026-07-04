# input — Layer 1 (services). Device abstraction (keyboard/mouse/gamepad state
# with per-frame edge detection) and rebindable action mapping (named button and
# axis actions over physical inputs). A sink for raw device events, so it stays
# independent of any windowing backend. Header-only. Depends on core.
# Namespace zukiru::input.
add_zukiru_module(input PUBLIC_DEPS core)

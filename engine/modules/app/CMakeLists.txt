# app — Layer 4 (needed early). The engine host: owns the window, GPU device and
# input state, wires them together, and runs the main loop that pumps events,
# updates input, and clears/presents each frame. Depends on core + platform +
# input + render. Namespace zukiru::app.
add_zukiru_module(app PUBLIC_DEPS core platform input render)

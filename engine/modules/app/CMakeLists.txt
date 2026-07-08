# app — Layer 4 (needed early). The engine host: owns the window, GPU device and
# input state, wires them together, and runs the main loop that pumps events,
# updates input, and clears/presents each frame. Depends on core + platform +
# input + render. Namespace zuki::app.
add_zuki_module(app PUBLIC_DEPS core platform input render)

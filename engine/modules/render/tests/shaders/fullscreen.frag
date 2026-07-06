#version 450
// Sample an offscreen render target's color attachment onto the screen.
layout(set = 0, binding = 0) uniform sampler2D tex;
layout(location = 0) in vec2 vUv;
layout(location = 0) out vec4 outColor;
void main() { outColor = texture(tex, vUv); }

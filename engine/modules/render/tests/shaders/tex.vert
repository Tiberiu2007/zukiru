#version 450
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUv;
layout(set = 0, binding = 0) uniform Transform { mat4 mvp; } u;
layout(location = 0) out vec2 vUv;
void main() {
    gl_Position = u.mvp * vec4(inPos, 0.0, 1.0);
    vUv = inUv;
}

#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(set = 0, binding = 0) uniform Transform { mat4 mvp; mat4 model; } u;
layout(location = 0) out vec2 vUv;
layout(location = 1) out vec3 vNormal;
void main() {
    gl_Position = u.mvp * vec4(inPos, 1.0);
    vUv = inUv;
    vNormal = mat3(u.model) * inNormal;
}

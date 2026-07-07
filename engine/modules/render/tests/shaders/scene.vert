#version 450
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
// Per-frame, shared by every object (ring-buffered uniform).
layout(set = 0, binding = 0) uniform Camera { mat4 viewProj; } cam;
// Per-draw, cheap (push constant).
layout(push_constant) uniform Push { mat4 model; } pc;
layout(location = 0) out vec2 vUv;
layout(location = 1) out vec3 vNormal;
void main() {
    gl_Position = cam.viewProj * pc.model * vec4(inPos, 1.0);
    vUv = inUv;
    vNormal = mat3(pc.model) * inNormal;
}

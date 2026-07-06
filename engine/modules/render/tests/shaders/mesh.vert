#version 450
// User-geometry triangle: vertex buffer supplies position + color.
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec3 vColor;
void main() {
    gl_Position = vec4(inPos, 0.0, 1.0);
    vColor = inColor;
}

#version 450
layout(set = 0, binding = 1) uniform sampler2D tex;
layout(location = 0) in vec2 vUv;
layout(location = 1) in vec3 vNormal;
layout(location = 0) out vec4 outColor;
void main() {
    // Simple directional light so the cube's faces read as distinct in 3D.
    const vec3 lightDir = normalize(vec3(0.4, 0.8, 0.6));
    float diffuse = max(dot(normalize(vNormal), lightDir), 0.0);
    float shade = 0.25 + 0.75 * diffuse;
    outColor = vec4(texture(tex, vUv).rgb * shade, 1.0);
}

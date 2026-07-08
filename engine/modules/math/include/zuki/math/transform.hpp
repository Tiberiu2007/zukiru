// TRS transforms plus view/projection matrix builders.
//
// Convention: right-handed world space, column-vector math (p' = M * p).
// Projections target a clip-space depth range of [0, 1] (Vulkan/D3D style). The
// render backend is responsible for any clip-space Y flip.
#pragma once

#include <zuki/core/types.hpp>
#include <zuki/math/mat.hpp>
#include <zuki/math/quat.hpp>
#include <zuki/math/scalar.hpp>
#include <zuki/math/vec.hpp>

#include <cmath>

namespace zuki::math {

// Position / rotation / scale bundle. `toMatrix()` composes T * R * S.
struct Transform {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Quat rotation = Quat::identity();
    Vec3 scale{1.0f, 1.0f, 1.0f};

    [[nodiscard]] Mat4 toMatrix() const noexcept {
        return Mat4::translation(position) * toMat4(rotation) * Mat4::scale(scale);
    }

    [[nodiscard]] Vec3 transformPoint(Vec3 p) const noexcept {
        return position + rotation.rotate(p * scale);
    }
};

// Right-handed view matrix looking from `eye` toward `center`.
[[nodiscard]] inline Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up) noexcept {
    const Vec3 f = normalize(center - eye);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);
    return Mat4::fromColumns({s.x, u.x, -f.x, 0.0f}, {s.y, u.y, -f.y, 0.0f},
                             {s.z, u.z, -f.z, 0.0f},
                             {-dot(s, eye), -dot(u, eye), dot(f, eye), 1.0f});
}

// Right-handed perspective projection, clip depth [0, 1].
// (`near`/`far` are avoided as identifiers — they are macros on some platforms.)
[[nodiscard]] inline Mat4 perspective(f32 fovYRadians, f32 aspect, f32 nearPlane,
                                      f32 farPlane) noexcept {
    const f32 f = 1.0f / std::tan(fovYRadians * 0.5f);
    return Mat4::fromColumns({f / aspect, 0.0f, 0.0f, 0.0f}, {0.0f, f, 0.0f, 0.0f},
                             {0.0f, 0.0f, farPlane / (nearPlane - farPlane), -1.0f},
                             {0.0f, 0.0f, -(farPlane * nearPlane) / (farPlane - nearPlane), 0.0f});
}

// Right-handed orthographic projection, clip depth [0, 1].
[[nodiscard]] inline Mat4 orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 nearPlane,
                                       f32 farPlane) noexcept {
    return Mat4::fromColumns(
        {2.0f / (right - left), 0.0f, 0.0f, 0.0f}, {0.0f, 2.0f / (top - bottom), 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f / (farPlane - nearPlane), 0.0f},
        {-(right + left) / (right - left), -(top + bottom) / (top - bottom),
         -nearPlane / (farPlane - nearPlane), 1.0f});
}

}  // namespace zuki::math

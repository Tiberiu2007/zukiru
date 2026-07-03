// Quat — unit quaternion for rotations. Layout (x, y, z, w) with w the scalar.
// Composition `a * b` applies b first, then a (same convention as matrices).
// Right-handed; rotating a vector uses q * v * q^-1.
#pragma once

#include <zukiru/core/types.hpp>
#include <zukiru/math/mat.hpp>
#include <zukiru/math/vec.hpp>

#include <cmath>

namespace zuki::math {

struct Quat {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 1.0f;  // identity

    constexpr Quat() = default;
    constexpr Quat(f32 xi, f32 yi, f32 zi, f32 wi) noexcept : x(xi), y(yi), z(zi), w(wi) {}

    static constexpr Quat identity() noexcept { return {}; }

    // Rotation of `angleRad` about a unit `axis`.
    static Quat fromAxisAngle(Vec3 axis, f32 angleRad) noexcept {
        const f32 half = angleRad * 0.5f;
        const f32 s = std::sin(half);
        return {axis.x * s, axis.y * s, axis.z * s, std::cos(half)};
    }

    // Intrinsic Tait-Bryan angles (radians): applied Z (roll), then X (pitch),
    // then Y (yaw): q = yaw * pitch * roll.
    static Quat fromEuler(f32 pitchX, f32 yawY, f32 rollZ) noexcept {
        return fromAxisAngle(Vec3::unitY(), yawY) * fromAxisAngle(Vec3::unitX(), pitchX) *
               fromAxisAngle(Vec3::unitZ(), rollZ);
    }

    friend constexpr Quat operator*(Quat a, Quat b) noexcept {
        return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
    }
    friend constexpr Quat operator*(Quat q, f32 s) noexcept {
        return {q.x * s, q.y * s, q.z * s, q.w * s};
    }
    friend constexpr Quat operator+(Quat a, Quat b) noexcept {
        return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
    }
    friend constexpr bool operator==(Quat a, Quat b) noexcept {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    }

    // Rotate a vector by this (unit) quaternion.
    [[nodiscard]] constexpr Vec3 rotate(Vec3 v) const noexcept {
        const Vec3 u{x, y, z};
        const Vec3 t = cross(u, v) * 2.0f;
        return v + t * w + cross(u, t);
    }
};

[[nodiscard]] constexpr f32 dot(Quat a, Quat b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
[[nodiscard]] constexpr Quat conjugate(Quat q) noexcept {
    return {-q.x, -q.y, -q.z, q.w};
}
[[nodiscard]] inline f32 length(Quat q) noexcept {
    return std::sqrt(dot(q, q));
}
[[nodiscard]] inline Quat normalize(Quat q) noexcept {
    const f32 len = length(q);
    return len > 0.0f ? q * (1.0f / len) : Quat::identity();
}
// Inverse rotation. For a unit quaternion this equals the conjugate.
[[nodiscard]] constexpr Quat inverse(Quat q) noexcept {
    const f32 d = dot(q, q);
    if (d == 0.0f) return Quat::identity();
    const Quat c = conjugate(q);
    return c * (1.0f / d);
}

// Convert a (unit) quaternion to a rotation matrix.
[[nodiscard]] constexpr Mat3 toMat3(Quat q) noexcept {
    const f32 xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
    const f32 xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
    const f32 wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
    return Mat3::fromColumns({1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy)},
                             {2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx)},
                             {2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy)});
}
[[nodiscard]] constexpr Mat4 toMat4(Quat q) noexcept {
    return Mat4::fromMat3(toMat3(q));
}

// Spherical linear interpolation between unit quaternions.
[[nodiscard]] inline Quat slerp(Quat a, Quat b, f32 t) noexcept {
    f32 d = dot(a, b);
    // Take the shorter arc.
    if (d < 0.0f) {
        b = b * -1.0f;
        d = -d;
    }
    constexpr f32 kLinearThreshold = 0.9995f;
    if (d > kLinearThreshold) {  // nearly parallel: fall back to linear + renormalize
        return normalize(a + (b + a * -1.0f) * t);
    }
    const f32 theta0 = std::acos(d);
    const f32 theta = theta0 * t;
    const f32 sinTheta0 = std::sin(theta0);
    const f32 s0 = std::sin(theta0 - theta) / sinTheta0;
    const f32 s1 = std::sin(theta) / sinTheta0;
    return a * s0 + b * s1;
}
[[nodiscard]] inline bool approxEqual(Quat a, Quat b, f32 epsilon = kEpsilon) noexcept {
    return approxEqual(a.x, b.x, epsilon) && approxEqual(a.y, b.y, epsilon) &&
           approxEqual(a.z, b.z, epsilon) && approxEqual(a.w, b.w, epsilon);
}

}  // namespace zuki::math

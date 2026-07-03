// Vec2 / Vec3 / Vec4 — f32 vectors and their free-function algebra.
//
// Concrete f32 types (not templated on scalar) keep the API readable and the
// common case fast; Vec4 is 16-byte aligned so it can later be backed by SIMD
// without an ABI change.
#pragma once

#include <zukiru/core/types.hpp>
#include <zukiru/math/scalar.hpp>

#include <cmath>

namespace zuki::math {

// =========================================================================
// Vec2
// =========================================================================
struct Vec2 {
    f32 x = 0.0f;
    f32 y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(f32 xi, f32 yi) noexcept : x(xi), y(yi) {}
    explicit constexpr Vec2(f32 s) noexcept : x(s), y(s) {}

    [[nodiscard]] constexpr f32& operator[](usize i) noexcept { return (&x)[i]; }
    [[nodiscard]] constexpr f32 operator[](usize i) const noexcept { return (&x)[i]; }

    constexpr Vec2 operator-() const noexcept { return {-x, -y}; }
    constexpr Vec2& operator+=(Vec2 o) noexcept { x += o.x; y += o.y; return *this; }
    constexpr Vec2& operator-=(Vec2 o) noexcept { x -= o.x; y -= o.y; return *this; }
    constexpr Vec2& operator*=(f32 s) noexcept { x *= s; y *= s; return *this; }
    constexpr Vec2& operator/=(f32 s) noexcept { x /= s; y /= s; return *this; }

    friend constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept { return {a.x + b.x, a.y + b.y}; }
    friend constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept { return {a.x - b.x, a.y - b.y}; }
    friend constexpr Vec2 operator*(Vec2 a, Vec2 b) noexcept { return {a.x * b.x, a.y * b.y}; }
    friend constexpr Vec2 operator*(Vec2 v, f32 s) noexcept { return {v.x * s, v.y * s}; }
    friend constexpr Vec2 operator*(f32 s, Vec2 v) noexcept { return {v.x * s, v.y * s}; }
    friend constexpr Vec2 operator/(Vec2 v, f32 s) noexcept { return {v.x / s, v.y / s}; }
    friend constexpr bool operator==(Vec2 a, Vec2 b) noexcept { return a.x == b.x && a.y == b.y; }
};

[[nodiscard]] constexpr f32 dot(Vec2 a, Vec2 b) noexcept {
    return a.x * b.x + a.y * b.y;
}
[[nodiscard]] constexpr f32 lengthSquared(Vec2 v) noexcept {
    return dot(v, v);
}
[[nodiscard]] inline f32 length(Vec2 v) noexcept {
    return std::sqrt(lengthSquared(v));
}
[[nodiscard]] inline Vec2 normalize(Vec2 v) noexcept {
    const f32 len = length(v);
    return len > 0.0f ? v / len : Vec2{};
}
[[nodiscard]] constexpr Vec2 lerp(Vec2 a, Vec2 b, f32 t) noexcept {
    return a + (b - a) * t;
}

// =========================================================================
// Vec3
// =========================================================================
struct Vec3 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(f32 xi, f32 yi, f32 zi) noexcept : x(xi), y(yi), z(zi) {}
    explicit constexpr Vec3(f32 s) noexcept : x(s), y(s), z(s) {}
    constexpr Vec3(Vec2 xy, f32 zi) noexcept : x(xy.x), y(xy.y), z(zi) {}

    [[nodiscard]] constexpr f32& operator[](usize i) noexcept { return (&x)[i]; }
    [[nodiscard]] constexpr f32 operator[](usize i) const noexcept { return (&x)[i]; }
    [[nodiscard]] constexpr Vec2 xy() const noexcept { return {x, y}; }

    constexpr Vec3 operator-() const noexcept { return {-x, -y, -z}; }
    constexpr Vec3& operator+=(Vec3 o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr Vec3& operator-=(Vec3 o) noexcept { x -= o.x; y -= o.y; z -= o.z; return *this; }
    constexpr Vec3& operator*=(f32 s) noexcept { x *= s; y *= s; z *= s; return *this; }
    constexpr Vec3& operator/=(f32 s) noexcept { x /= s; y /= s; z /= s; return *this; }

    friend constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }
    friend constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }
    friend constexpr Vec3 operator*(Vec3 a, Vec3 b) noexcept {
        return {a.x * b.x, a.y * b.y, a.z * b.z};
    }
    friend constexpr Vec3 operator*(Vec3 v, f32 s) noexcept { return {v.x * s, v.y * s, v.z * s}; }
    friend constexpr Vec3 operator*(f32 s, Vec3 v) noexcept { return {v.x * s, v.y * s, v.z * s}; }
    friend constexpr Vec3 operator/(Vec3 v, f32 s) noexcept { return {v.x / s, v.y / s, v.z / s}; }
    friend constexpr bool operator==(Vec3 a, Vec3 b) noexcept {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    // Common axes / constants.
    static constexpr Vec3 zero() noexcept { return {0.0f, 0.0f, 0.0f}; }
    static constexpr Vec3 one() noexcept { return {1.0f, 1.0f, 1.0f}; }
    static constexpr Vec3 unitX() noexcept { return {1.0f, 0.0f, 0.0f}; }
    static constexpr Vec3 unitY() noexcept { return {0.0f, 1.0f, 0.0f}; }
    static constexpr Vec3 unitZ() noexcept { return {0.0f, 0.0f, 1.0f}; }
};

[[nodiscard]] constexpr f32 dot(Vec3 a, Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
[[nodiscard]] constexpr Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
[[nodiscard]] constexpr f32 lengthSquared(Vec3 v) noexcept {
    return dot(v, v);
}
[[nodiscard]] inline f32 length(Vec3 v) noexcept {
    return std::sqrt(lengthSquared(v));
}
[[nodiscard]] inline f32 distance(Vec3 a, Vec3 b) noexcept {
    return length(b - a);
}
[[nodiscard]] inline Vec3 normalize(Vec3 v) noexcept {
    const f32 len = length(v);
    return len > 0.0f ? v / len : Vec3{};
}
[[nodiscard]] constexpr Vec3 lerp(Vec3 a, Vec3 b, f32 t) noexcept {
    return a + (b - a) * t;
}
// Reflect `v` about a (unit) `normal`.
[[nodiscard]] constexpr Vec3 reflect(Vec3 v, Vec3 normal) noexcept {
    return v - normal * (2.0f * dot(v, normal));
}
[[nodiscard]] constexpr Vec3 minComponents(Vec3 a, Vec3 b) noexcept {
    return {min(a.x, b.x), min(a.y, b.y), min(a.z, b.z)};
}
[[nodiscard]] constexpr Vec3 maxComponents(Vec3 a, Vec3 b) noexcept {
    return {max(a.x, b.x), max(a.y, b.y), max(a.z, b.z)};
}
[[nodiscard]] inline bool approxEqual(Vec3 a, Vec3 b, f32 epsilon = kEpsilon) noexcept {
    return approxEqual(a.x, b.x, epsilon) && approxEqual(a.y, b.y, epsilon) &&
           approxEqual(a.z, b.z, epsilon);
}

// =========================================================================
// Vec4 (16-byte aligned; SIMD-ready)
// =========================================================================
struct alignas(16) Vec4 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;
    f32 w = 0.0f;

    constexpr Vec4() = default;
    constexpr Vec4(f32 xi, f32 yi, f32 zi, f32 wi) noexcept : x(xi), y(yi), z(zi), w(wi) {}
    explicit constexpr Vec4(f32 s) noexcept : x(s), y(s), z(s), w(s) {}
    constexpr Vec4(Vec3 xyz, f32 wi) noexcept : x(xyz.x), y(xyz.y), z(xyz.z), w(wi) {}

    [[nodiscard]] constexpr f32& operator[](usize i) noexcept { return (&x)[i]; }
    [[nodiscard]] constexpr f32 operator[](usize i) const noexcept { return (&x)[i]; }
    [[nodiscard]] constexpr Vec3 xyz() const noexcept { return {x, y, z}; }

    constexpr Vec4 operator-() const noexcept { return {-x, -y, -z, -w}; }
    constexpr Vec4& operator+=(Vec4 o) noexcept {
        x += o.x; y += o.y; z += o.z; w += o.w;
        return *this;
    }
    constexpr Vec4& operator-=(Vec4 o) noexcept {
        x -= o.x; y -= o.y; z -= o.z; w -= o.w;
        return *this;
    }
    constexpr Vec4& operator*=(f32 s) noexcept {
        x *= s; y *= s; z *= s; w *= s;
        return *this;
    }

    friend constexpr Vec4 operator+(Vec4 a, Vec4 b) noexcept {
        return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
    }
    friend constexpr Vec4 operator-(Vec4 a, Vec4 b) noexcept {
        return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
    }
    friend constexpr Vec4 operator*(Vec4 v, f32 s) noexcept {
        return {v.x * s, v.y * s, v.z * s, v.w * s};
    }
    friend constexpr Vec4 operator*(f32 s, Vec4 v) noexcept {
        return {v.x * s, v.y * s, v.z * s, v.w * s};
    }
    friend constexpr bool operator==(Vec4 a, Vec4 b) noexcept {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    }
};

[[nodiscard]] constexpr f32 dot(Vec4 a, Vec4 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
[[nodiscard]] constexpr f32 lengthSquared(Vec4 v) noexcept {
    return dot(v, v);
}
[[nodiscard]] inline f32 length(Vec4 v) noexcept {
    return std::sqrt(lengthSquared(v));
}
[[nodiscard]] inline Vec4 normalize(Vec4 v) noexcept {
    const f32 len = length(v);
    return len > 0.0f ? v * (1.0f / len) : Vec4{};
}
[[nodiscard]] constexpr Vec4 lerp(Vec4 a, Vec4 b, f32 t) noexcept {
    return a + (b - a) * t;
}

}  // namespace zuki::math

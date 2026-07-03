// Mat3 / Mat4 — column-major, column-vector convention (v' = M * v),
// right-handed. Storage is column-major (OpenGL/GLSL layout): element (row r,
// col c) lives at e[c*N + r], so `e` can be handed straight to graphics APIs.
#pragma once

#include <zukiru/core/types.hpp>
#include <zukiru/math/vec.hpp>

namespace zukiru::math {

// =========================================================================
// Mat3 — rotation / scale / normal matrices.
// =========================================================================
struct Mat3 {
    // Column-major: e[col * 3 + row].
    f32 e[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};

    constexpr Mat3() = default;

    [[nodiscard]] constexpr f32& operator()(usize row, usize col) noexcept {
        return e[col * 3 + row];
    }
    [[nodiscard]] constexpr f32 operator()(usize row, usize col) const noexcept {
        return e[col * 3 + row];
    }
    [[nodiscard]] constexpr Vec3 col(usize c) const noexcept {
        return {e[c * 3 + 0], e[c * 3 + 1], e[c * 3 + 2]};
    }

    static constexpr Mat3 identity() noexcept { return {}; }
    static constexpr Mat3 fromColumns(Vec3 c0, Vec3 c1, Vec3 c2) noexcept {
        Mat3 m;
        m.e[0] = c0.x; m.e[1] = c0.y; m.e[2] = c0.z;
        m.e[3] = c1.x; m.e[4] = c1.y; m.e[5] = c1.z;
        m.e[6] = c2.x; m.e[7] = c2.y; m.e[8] = c2.z;
        return m;
    }

    friend constexpr Vec3 operator*(const Mat3& m, Vec3 v) noexcept {
        return m.col(0) * v.x + m.col(1) * v.y + m.col(2) * v.z;
    }
    friend constexpr Mat3 operator*(const Mat3& a, const Mat3& b) noexcept {
        return fromColumns(a * b.col(0), a * b.col(1), a * b.col(2));
    }
};

[[nodiscard]] constexpr Mat3 transpose(const Mat3& m) noexcept {
    Mat3 r;
    for (usize c = 0; c < 3; ++c)
        for (usize row = 0; row < 3; ++row) r(c, row) = m(row, c);
    return r;
}
[[nodiscard]] constexpr f32 determinant(const Mat3& m) noexcept {
    return m(0, 0) * (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) -
           m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0)) +
           m(0, 2) * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));
}
// Inverse; returns identity if the matrix is singular.
[[nodiscard]] constexpr Mat3 inverse(const Mat3& m) noexcept {
    const f32 det = determinant(m);
    if (det == 0.0f) return Mat3::identity();
    const f32 inv = 1.0f / det;
    Mat3 r;
    r(0, 0) = (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) * inv;
    r(0, 1) = (m(0, 2) * m(2, 1) - m(0, 1) * m(2, 2)) * inv;
    r(0, 2) = (m(0, 1) * m(1, 2) - m(0, 2) * m(1, 1)) * inv;
    r(1, 0) = (m(1, 2) * m(2, 0) - m(1, 0) * m(2, 2)) * inv;
    r(1, 1) = (m(0, 0) * m(2, 2) - m(0, 2) * m(2, 0)) * inv;
    r(1, 2) = (m(0, 2) * m(1, 0) - m(0, 0) * m(1, 2)) * inv;
    r(2, 0) = (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0)) * inv;
    r(2, 1) = (m(0, 1) * m(2, 0) - m(0, 0) * m(2, 1)) * inv;
    r(2, 2) = (m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0)) * inv;
    return r;
}

// =========================================================================
// Mat4 — full affine / projective transforms.
// =========================================================================
struct alignas(16) Mat4 {
    // Column-major: e[col * 4 + row]. Defaults to identity.
    f32 e[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};

    constexpr Mat4() = default;

    [[nodiscard]] constexpr f32& operator()(usize row, usize col) noexcept {
        return e[col * 4 + row];
    }
    [[nodiscard]] constexpr f32 operator()(usize row, usize col) const noexcept {
        return e[col * 4 + row];
    }
    [[nodiscard]] constexpr Vec4 col(usize c) const noexcept {
        return {e[c * 4 + 0], e[c * 4 + 1], e[c * 4 + 2], e[c * 4 + 3]};
    }

    static constexpr Mat4 identity() noexcept { return {}; }
    static constexpr Mat4 fromColumns(Vec4 c0, Vec4 c1, Vec4 c2, Vec4 c3) noexcept {
        Mat4 m;
        m.e[0] = c0.x;  m.e[1] = c0.y;   m.e[2] = c0.z;   m.e[3] = c0.w;
        m.e[4] = c1.x;  m.e[5] = c1.y;   m.e[6] = c1.z;   m.e[7] = c1.w;
        m.e[8] = c2.x;  m.e[9] = c2.y;   m.e[10] = c2.z;  m.e[11] = c2.w;
        m.e[12] = c3.x; m.e[13] = c3.y;  m.e[14] = c3.z;  m.e[15] = c3.w;
        return m;
    }

    friend constexpr Vec4 operator*(const Mat4& m, Vec4 v) noexcept {
        return m.col(0) * v.x + m.col(1) * v.y + m.col(2) * v.z + m.col(3) * v.w;
    }
    friend constexpr Mat4 operator*(const Mat4& a, const Mat4& b) noexcept {
        return fromColumns(a * b.col(0), a * b.col(1), a * b.col(2), a * b.col(3));
    }

    // Transform a position (implicit w = 1, applies translation).
    [[nodiscard]] constexpr Vec3 transformPoint(Vec3 p) const noexcept {
        const Vec4 r = *this * Vec4{p, 1.0f};
        return {r.x, r.y, r.z};
    }
    // Transform a direction (implicit w = 0, ignores translation).
    [[nodiscard]] constexpr Vec3 transformDirection(Vec3 d) const noexcept {
        const Vec4 r = *this * Vec4{d, 0.0f};
        return {r.x, r.y, r.z};
    }

    // --- Affine builders -------------------------------------------------
    static constexpr Mat4 translation(Vec3 t) noexcept {
        Mat4 m;
        m.e[12] = t.x; m.e[13] = t.y; m.e[14] = t.z;
        return m;
    }
    static constexpr Mat4 scale(Vec3 s) noexcept {
        Mat4 m;
        m.e[0] = s.x; m.e[5] = s.y; m.e[10] = s.z;
        return m;
    }
    static constexpr Mat4 fromMat3(const Mat3& r) noexcept {
        Mat4 m;
        for (usize c = 0; c < 3; ++c)
            for (usize row = 0; row < 3; ++row) m(row, c) = r(row, c);
        return m;
    }
};

[[nodiscard]] constexpr Mat3 toMat3(const Mat4& m) noexcept {
    return Mat3::fromColumns(m.col(0).xyz(), m.col(1).xyz(), m.col(2).xyz());
}

[[nodiscard]] constexpr Mat4 transpose(const Mat4& m) noexcept {
    Mat4 r;
    for (usize c = 0; c < 4; ++c)
        for (usize row = 0; row < 4; ++row) r(c, row) = m(row, c);
    return r;
}

[[nodiscard]] constexpr f32 determinant(const Mat4& m) noexcept {
    const f32* a = m.e;
    const f32 s0 = a[0] * a[5] - a[4] * a[1];
    const f32 s1 = a[0] * a[6] - a[4] * a[2];
    const f32 s2 = a[0] * a[7] - a[4] * a[3];
    const f32 s3 = a[1] * a[6] - a[5] * a[2];
    const f32 s4 = a[1] * a[7] - a[5] * a[3];
    const f32 s5 = a[2] * a[7] - a[6] * a[3];
    const f32 c5 = a[10] * a[15] - a[14] * a[11];
    const f32 c4 = a[9] * a[15] - a[13] * a[11];
    const f32 c3 = a[9] * a[14] - a[13] * a[10];
    const f32 c2 = a[8] * a[15] - a[12] * a[11];
    const f32 c1 = a[8] * a[14] - a[12] * a[10];
    const f32 c0 = a[8] * a[13] - a[12] * a[9];
    return s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
}

// Full 4x4 inverse (adjugate / determinant). Returns identity if singular.
[[nodiscard]] constexpr Mat4 inverse(const Mat4& m) noexcept {
    const f32* a = m.e;
    const f32 s0 = a[0] * a[5] - a[4] * a[1];
    const f32 s1 = a[0] * a[6] - a[4] * a[2];
    const f32 s2 = a[0] * a[7] - a[4] * a[3];
    const f32 s3 = a[1] * a[6] - a[5] * a[2];
    const f32 s4 = a[1] * a[7] - a[5] * a[3];
    const f32 s5 = a[2] * a[7] - a[6] * a[3];
    const f32 c5 = a[10] * a[15] - a[14] * a[11];
    const f32 c4 = a[9] * a[15] - a[13] * a[11];
    const f32 c3 = a[9] * a[14] - a[13] * a[10];
    const f32 c2 = a[8] * a[15] - a[12] * a[11];
    const f32 c1 = a[8] * a[14] - a[12] * a[10];
    const f32 c0 = a[8] * a[13] - a[12] * a[9];

    const f32 det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
    if (det == 0.0f) return Mat4::identity();
    const f32 invDet = 1.0f / det;

    Mat4 r;
    f32* b = r.e;
    b[0] = (a[5] * c5 - a[6] * c4 + a[7] * c3) * invDet;
    b[1] = (-a[1] * c5 + a[2] * c4 - a[3] * c3) * invDet;
    b[2] = (a[13] * s5 - a[14] * s4 + a[15] * s3) * invDet;
    b[3] = (-a[9] * s5 + a[10] * s4 - a[11] * s3) * invDet;
    b[4] = (-a[4] * c5 + a[6] * c2 - a[7] * c1) * invDet;
    b[5] = (a[0] * c5 - a[2] * c2 + a[3] * c1) * invDet;
    b[6] = (-a[12] * s5 + a[14] * s2 - a[15] * s1) * invDet;
    b[7] = (a[8] * s5 - a[10] * s2 + a[11] * s1) * invDet;
    b[8] = (a[4] * c4 - a[5] * c2 + a[7] * c0) * invDet;
    b[9] = (-a[0] * c4 + a[1] * c2 - a[3] * c0) * invDet;
    b[10] = (a[12] * s4 - a[13] * s2 + a[15] * s0) * invDet;
    b[11] = (-a[8] * s4 + a[9] * s2 - a[11] * s0) * invDet;
    b[12] = (-a[4] * c3 + a[5] * c1 - a[6] * c0) * invDet;
    b[13] = (a[0] * c3 - a[1] * c1 + a[2] * c0) * invDet;
    b[14] = (-a[12] * s3 + a[13] * s1 - a[14] * s0) * invDet;
    b[15] = (a[8] * s3 - a[9] * s1 + a[10] * s0) * invDet;
    return r;
}

[[nodiscard]] inline bool approxEqual(const Mat4& a, const Mat4& b, f32 epsilon = kEpsilon) noexcept {
    for (usize i = 0; i < 16; ++i)
        if (!approxEqual(a.e[i], b.e[i], epsilon)) return false;
    return true;
}

}  // namespace zukiru::math

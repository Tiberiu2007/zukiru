// Scalar constants and helpers. All f32-first (the engine's runtime scalar).
#pragma once

#include <zuki/core/types.hpp>

#include <cmath>

namespace zuki::math {

// --- Constants -----------------------------------------------------------
inline constexpr f32 kPi = 3.14159265358979323846f;
inline constexpr f32 kTwoPi = 2.0f * kPi;
inline constexpr f32 kHalfPi = 0.5f * kPi;
inline constexpr f32 kInvPi = 1.0f / kPi;
inline constexpr f32 kDegToRad = kPi / 180.0f;
inline constexpr f32 kRadToDeg = 180.0f / kPi;

// Default tolerance for approximate float comparisons.
inline constexpr f32 kEpsilon = 1e-6f;

// --- Angle conversion ----------------------------------------------------
[[nodiscard]] constexpr f32 radians(f32 degrees) noexcept {
    return degrees * kDegToRad;
}
[[nodiscard]] constexpr f32 degrees(f32 radians) noexcept {
    return radians * kRadToDeg;
}

// --- Generic min/max/clamp ----------------------------------------------
template <class T>
[[nodiscard]] constexpr const T& min(const T& a, const T& b) noexcept {
    return b < a ? b : a;
}
template <class T>
[[nodiscard]] constexpr const T& max(const T& a, const T& b) noexcept {
    return a < b ? b : a;
}
template <class T>
[[nodiscard]] constexpr T clamp(T v, T lo, T hi) noexcept {
    return v < lo ? lo : (hi < v ? hi : v);
}

// --- f32 helpers ---------------------------------------------------------
[[nodiscard]] constexpr f32 lerp(f32 a, f32 b, f32 t) noexcept {
    return a + (b - a) * t;
}
[[nodiscard]] constexpr f32 saturate(f32 v) noexcept {
    return clamp(v, 0.0f, 1.0f);
}
[[nodiscard]] constexpr f32 sign(f32 v) noexcept {
    return v < 0.0f ? -1.0f : (v > 0.0f ? 1.0f : 0.0f);
}
[[nodiscard]] inline bool approxEqual(f32 a, f32 b, f32 epsilon = kEpsilon) noexcept {
    return std::fabs(a - b) <= epsilon;
}
[[nodiscard]] inline bool approxZero(f32 v, f32 epsilon = kEpsilon) noexcept {
    return std::fabs(v) <= epsilon;
}

}  // namespace zuki::math

// Bounding volumes and ray-cast primitives: Aabb, Ray, Plane, Sphere, plus
// ray/volume intersection helpers. Right-handed, f32.
#pragma once

#include <zuki/core/types.hpp>
#include <zuki/math/scalar.hpp>
#include <zuki/math/vec.hpp>

#include <cmath>
#include <limits>
#include <optional>

namespace zuki::math {

// Axis-aligned bounding box.
struct Aabb {
    Vec3 min{0.0f, 0.0f, 0.0f};
    Vec3 max{0.0f, 0.0f, 0.0f};

    static constexpr Aabb fromCenterExtents(Vec3 center, Vec3 halfExtents) noexcept {
        return {center - halfExtents, center + halfExtents};
    }

    [[nodiscard]] constexpr Vec3 center() const noexcept { return (min + max) * 0.5f; }
    [[nodiscard]] constexpr Vec3 size() const noexcept { return max - min; }
    [[nodiscard]] constexpr Vec3 halfExtents() const noexcept { return (max - min) * 0.5f; }

    [[nodiscard]] constexpr bool contains(Vec3 p) const noexcept {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z &&
               p.z <= max.z;
    }
    [[nodiscard]] constexpr bool intersects(const Aabb& o) const noexcept {
        return min.x <= o.max.x && max.x >= o.min.x && min.y <= o.max.y && max.y >= o.min.y &&
               min.z <= o.max.z && max.z >= o.min.z;
    }
    // Grow to include a point.
    constexpr void expand(Vec3 p) noexcept {
        min = minComponents(min, p);
        max = maxComponents(max, p);
    }
    // Grow to include another box.
    constexpr void merge(const Aabb& o) noexcept {
        min = minComponents(min, o.min);
        max = maxComponents(max, o.max);
    }
};

// Ray with a (normalized) direction.
struct Ray {
    Vec3 origin{0.0f, 0.0f, 0.0f};
    Vec3 direction{0.0f, 0.0f, 1.0f};

    [[nodiscard]] constexpr Vec3 at(f32 t) const noexcept { return origin + direction * t; }
};

// Plane defined by dot(normal, p) + d == 0.
struct Plane {
    Vec3 normal{0.0f, 1.0f, 0.0f};
    f32 d = 0.0f;

    static Plane fromPointNormal(Vec3 point, Vec3 unitNormal) noexcept {
        return {unitNormal, -dot(unitNormal, point)};
    }
    [[nodiscard]] constexpr f32 signedDistance(Vec3 p) const noexcept {
        return dot(normal, p) + d;
    }
};

// Bounding sphere.
struct Sphere {
    Vec3 center{0.0f, 0.0f, 0.0f};
    f32 radius = 0.0f;

    [[nodiscard]] constexpr bool contains(Vec3 p) const noexcept {
        return lengthSquared(p - center) <= radius * radius;
    }
    [[nodiscard]] constexpr bool intersects(const Sphere& o) const noexcept {
        const f32 r = radius + o.radius;
        return lengthSquared(o.center - center) <= r * r;
    }
};

// --- Ray casts (return distance t along the ray to the nearest hit) -------
[[nodiscard]] inline std::optional<f32> raySphere(const Ray& ray, const Sphere& s) noexcept {
    const Vec3 oc = ray.origin - s.center;
    const f32 b = dot(oc, ray.direction);
    const f32 c = lengthSquared(oc) - s.radius * s.radius;
    const f32 disc = b * b - c;  // assumes ray.direction is unit length
    if (disc < 0.0f) return std::nullopt;
    const f32 sq = std::sqrt(disc);
    f32 t = -b - sq;
    if (t < 0.0f) t = -b + sq;  // origin inside the sphere: use the far root
    if (t < 0.0f) return std::nullopt;
    return t;
}

[[nodiscard]] inline std::optional<f32> rayPlane(const Ray& ray, const Plane& p) noexcept {
    const f32 denom = dot(p.normal, ray.direction);
    if (approxZero(denom)) return std::nullopt;  // parallel
    const f32 t = -(dot(p.normal, ray.origin) + p.d) / denom;
    if (t < 0.0f) return std::nullopt;
    return t;
}

[[nodiscard]] inline std::optional<f32> rayAabb(const Ray& ray, const Aabb& box) noexcept {
    f32 tMin = 0.0f;
    f32 tMax = std::numeric_limits<f32>::max();
    for (usize i = 0; i < 3; ++i) {
        const f32 invD = 1.0f / ray.direction[i];
        f32 t0 = (box.min[i] - ray.origin[i]) * invD;
        f32 t1 = (box.max[i] - ray.origin[i]) * invD;
        if (invD < 0.0f) {
            const f32 tmp = t0;
            t0 = t1;
            t1 = tmp;
        }
        tMin = max(tMin, t0);
        tMax = min(tMax, t1);
        if (tMax < tMin) return std::nullopt;
    }
    return tMin;
}

}  // namespace zuki::math

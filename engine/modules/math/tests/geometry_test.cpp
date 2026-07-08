#include <zuki/math/geometry.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace zuki::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("Aabb containment, size and overlap", "[math][geometry]") {
    const Aabb box = Aabb::fromCenterExtents(Vec3{}, Vec3{1.0f, 1.0f, 1.0f});
    REQUIRE(box.contains(Vec3{0.5f, -0.5f, 0.0f}));
    REQUIRE_FALSE(box.contains(Vec3{2.0f, 0.0f, 0.0f}));
    REQUIRE(box.center() == Vec3{});
    REQUIRE(box.size() == Vec3{2.0f, 2.0f, 2.0f});

    const Aabb other = Aabb::fromCenterExtents(Vec3{1.5f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    REQUIRE(box.intersects(other));
    const Aabb far = Aabb::fromCenterExtents(Vec3{5.0f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    REQUIRE_FALSE(box.intersects(far));
}

TEST_CASE("Aabb expand and merge", "[math][geometry]") {
    Aabb box;  // degenerate at origin
    box.expand(Vec3{1.0f, 2.0f, 3.0f});
    box.expand(Vec3{-1.0f, 0.0f, -3.0f});
    REQUIRE(box.min == Vec3{-1.0f, 0.0f, -3.0f});
    REQUIRE(box.max == Vec3{1.0f, 2.0f, 3.0f});
}

TEST_CASE("ray vs sphere", "[math][geometry]") {
    const Sphere s{Vec3{0.0f, 0.0f, -5.0f}, 1.0f};
    const Ray hit{Vec3{}, Vec3{0.0f, 0.0f, -1.0f}};
    const auto t = raySphere(hit, s);
    REQUIRE(t.has_value());
    REQUIRE_THAT(*t, WithinAbs(4.0f, 1e-4f));  // hits front face at z=-4

    const Ray miss{Vec3{}, Vec3{0.0f, 1.0f, 0.0f}};
    REQUIRE_FALSE(raySphere(miss, s).has_value());
}

TEST_CASE("ray vs plane", "[math][geometry]") {
    const Plane ground = Plane::fromPointNormal(Vec3{}, Vec3::unitY());
    const Ray down{Vec3{0.0f, 10.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}};
    const auto t = rayPlane(down, ground);
    REQUIRE(t.has_value());
    REQUIRE_THAT(*t, WithinAbs(10.0f, 1e-4f));

    const Ray parallel{Vec3{0.0f, 10.0f, 0.0f}, Vec3::unitX()};
    REQUIRE_FALSE(rayPlane(parallel, ground).has_value());
}

TEST_CASE("plane signed distance", "[math][geometry]") {
    const Plane ground = Plane::fromPointNormal(Vec3{}, Vec3::unitY());
    REQUIRE_THAT(ground.signedDistance(Vec3{0.0f, 3.0f, 0.0f}), WithinAbs(3.0f, 1e-6f));
    REQUIRE_THAT(ground.signedDistance(Vec3{0.0f, -2.0f, 0.0f}), WithinAbs(-2.0f, 1e-6f));
}

TEST_CASE("ray vs aabb", "[math][geometry]") {
    const Aabb box = Aabb::fromCenterExtents(Vec3{0.0f, 0.0f, -5.0f}, Vec3{1.0f, 1.0f, 1.0f});
    const Ray hit{Vec3{}, Vec3{0.0f, 0.0f, -1.0f}};
    const auto t = rayAabb(hit, box);
    REQUIRE(t.has_value());
    REQUIRE_THAT(*t, WithinAbs(4.0f, 1e-4f));

    const Ray miss{Vec3{0.0f, 5.0f, 0.0f}, Vec3{0.0f, 0.0f, -1.0f}};
    REQUIRE_FALSE(rayAabb(miss, box).has_value());
}

TEST_CASE("sphere overlap", "[math][geometry]") {
    const Sphere a{Vec3{}, 1.0f};
    REQUIRE(a.intersects(Sphere{Vec3{1.5f, 0.0f, 0.0f}, 1.0f}));
    REQUIRE_FALSE(a.intersects(Sphere{Vec3{5.0f, 0.0f, 0.0f}, 1.0f}));
    REQUIRE(a.contains(Vec3{0.5f, 0.0f, 0.0f}));
}

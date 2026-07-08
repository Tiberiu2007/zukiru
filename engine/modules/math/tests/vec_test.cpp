#include <zuki/math/vec.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace zuki::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("Vec3 arithmetic", "[math][vec]") {
    constexpr Vec3 a{1.0f, 2.0f, 3.0f};
    constexpr Vec3 b{4.0f, 5.0f, 6.0f};
    STATIC_REQUIRE(a + b == Vec3{5.0f, 7.0f, 9.0f});
    STATIC_REQUIRE(b - a == Vec3{3.0f, 3.0f, 3.0f});
    STATIC_REQUIRE(a * 2.0f == Vec3{2.0f, 4.0f, 6.0f});
    STATIC_REQUIRE(2.0f * a == Vec3{2.0f, 4.0f, 6.0f});
    STATIC_REQUIRE(-a == Vec3{-1.0f, -2.0f, -3.0f});
    STATIC_REQUIRE(a * b == Vec3{4.0f, 10.0f, 18.0f});  // component-wise
}

TEST_CASE("Vec3 dot and cross", "[math][vec]") {
    STATIC_REQUIRE(dot(Vec3{1.0f, 2.0f, 3.0f}, Vec3{4.0f, 5.0f, 6.0f}) == 32.0f);
    // Right-handed: x cross y == z.
    STATIC_REQUIRE(cross(Vec3::unitX(), Vec3::unitY()) == Vec3::unitZ());
    STATIC_REQUIRE(cross(Vec3::unitY(), Vec3::unitZ()) == Vec3::unitX());
}

TEST_CASE("Vec3 length and normalize", "[math][vec]") {
    const Vec3 v{3.0f, 4.0f, 0.0f};
    REQUIRE(lengthSquared(v) == 25.0f);
    REQUIRE_THAT(length(v), WithinAbs(5.0f, 1e-6f));
    const Vec3 n = normalize(v);
    REQUIRE_THAT(length(n), WithinAbs(1.0f, 1e-6f));
    REQUIRE(approxEqual(n, Vec3{0.6f, 0.8f, 0.0f}));
    // Normalizing zero yields zero (no NaN).
    REQUIRE(normalize(Vec3{}) == Vec3{});
}

TEST_CASE("Vec3 reflect", "[math][vec]") {
    // Reflecting a downward ray off a floor (normal +Y) flips Y.
    const Vec3 r = reflect(Vec3{1.0f, -1.0f, 0.0f}, Vec3::unitY());
    REQUIRE(approxEqual(r, Vec3{1.0f, 1.0f, 0.0f}));
}

TEST_CASE("Vec3 indexing and swizzles", "[math][vec]") {
    Vec3 v{7.0f, 8.0f, 9.0f};
    REQUIRE(v[0] == 7.0f);
    v[1] = 80.0f;
    REQUIRE(v.y == 80.0f);
    REQUIRE(v.xy() == Vec2{7.0f, 80.0f});
}

TEST_CASE("Vec2 and Vec4 basics", "[math][vec]") {
    STATIC_REQUIRE(dot(Vec2{1.0f, 0.0f}, Vec2{0.0f, 1.0f}) == 0.0f);
    STATIC_REQUIRE(Vec4{Vec3{1.0f, 2.0f, 3.0f}, 4.0f}.xyz() == Vec3{1.0f, 2.0f, 3.0f});
    REQUIRE(alignof(Vec4) == 16);
    const Vec4 v{1.0f, 2.0f, 2.0f, 0.0f};
    REQUIRE_THAT(length(v), WithinAbs(3.0f, 1e-6f));
}

TEST_CASE("component-wise min/max", "[math][vec]") {
    STATIC_REQUIRE(minComponents(Vec3{1.0f, 5.0f, 3.0f}, Vec3{4.0f, 2.0f, 6.0f}) ==
                   Vec3{1.0f, 2.0f, 3.0f});
    STATIC_REQUIRE(maxComponents(Vec3{1.0f, 5.0f, 3.0f}, Vec3{4.0f, 2.0f, 6.0f}) ==
                   Vec3{4.0f, 5.0f, 6.0f});
}

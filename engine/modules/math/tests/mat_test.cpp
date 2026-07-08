#include <zuki/math/mat.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace zuki::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("Mat4 defaults to identity and is a multiplicative unit", "[math][mat]") {
    const Mat4 id;
    REQUIRE(id(0, 0) == 1.0f);
    REQUIRE(id(1, 0) == 0.0f);
    const Mat4 t = Mat4::translation({1.0f, 2.0f, 3.0f});
    REQUIRE(approxEqual(id * t, t));
    REQUIRE(approxEqual(t * id, t));
}

TEST_CASE("Mat4 translation moves a point but not a direction", "[math][mat]") {
    const Mat4 t = Mat4::translation({10.0f, 0.0f, -5.0f});
    REQUIRE(approxEqual(t.transformPoint(Vec3{1.0f, 1.0f, 1.0f}), Vec3{11.0f, 1.0f, -4.0f}));
    REQUIRE(approxEqual(t.transformDirection(Vec3{1.0f, 1.0f, 1.0f}), Vec3{1.0f, 1.0f, 1.0f}));
}

TEST_CASE("Mat4 scale", "[math][mat]") {
    const Mat4 s = Mat4::scale({2.0f, 3.0f, 4.0f});
    REQUIRE(approxEqual(s.transformPoint(Vec3{1.0f, 1.0f, 1.0f}), Vec3{2.0f, 3.0f, 4.0f}));
}

TEST_CASE("Mat4 multiply composes transforms (T then S applied right-to-left)",
          "[math][mat]") {
    const Mat4 t = Mat4::translation({5.0f, 0.0f, 0.0f});
    const Mat4 s = Mat4::scale({2.0f, 2.0f, 2.0f});
    // (t * s) scales first, then translates.
    const Vec3 p = (t * s).transformPoint(Vec3{1.0f, 0.0f, 0.0f});
    REQUIRE(approxEqual(p, Vec3{7.0f, 0.0f, 0.0f}));
}

TEST_CASE("Mat4 transpose swaps rows and columns", "[math][mat]") {
    Mat4 m;
    m(0, 3) = 9.0f;
    const Mat4 tr = transpose(m);
    REQUIRE(tr(3, 0) == 9.0f);
    REQUIRE(approxEqual(transpose(transpose(m)), m));
}

TEST_CASE("Mat4 inverse times original is identity", "[math][mat]") {
    const Mat4 m = Mat4::translation({3.0f, -2.0f, 1.0f}) * Mat4::scale({2.0f, 4.0f, 0.5f});
    const Mat4 inv = inverse(m);
    REQUIRE(approxEqual(m * inv, Mat4::identity(), 1e-4f));
    REQUIRE(approxEqual(inv * m, Mat4::identity(), 1e-4f));
}

TEST_CASE("determinant of a scale is the product of diagonal", "[math][mat]") {
    const Mat4 s = Mat4::scale({2.0f, 3.0f, 4.0f});
    REQUIRE_THAT(determinant(s), WithinAbs(24.0f, 1e-4f));
    REQUIRE_THAT(determinant(Mat4::identity()), WithinAbs(1.0f, 1e-6f));
}

TEST_CASE("singular matrix inverse returns identity (guarded)", "[math][mat]") {
    const Mat4 zeroScale = Mat4::scale({0.0f, 1.0f, 1.0f});
    REQUIRE(approxEqual(inverse(zeroScale), Mat4::identity()));
}

TEST_CASE("Mat3 inverse", "[math][mat]") {
    const Mat3 m = Mat3::fromColumns({2.0f, 0.0f, 0.0f}, {0.0f, 4.0f, 0.0f}, {0.0f, 0.0f, 5.0f});
    const Mat3 inv = inverse(m);
    const Mat3 prod = m * inv;
    REQUIRE_THAT(prod(0, 0), WithinAbs(1.0f, 1e-6f));
    REQUIRE_THAT(prod(1, 1), WithinAbs(1.0f, 1e-6f));
    REQUIRE_THAT(prod(2, 2), WithinAbs(1.0f, 1e-6f));
}

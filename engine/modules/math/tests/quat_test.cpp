#include <zuki/math/quat.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace zuki::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("identity quaternion rotates nothing", "[math][quat]") {
    const Vec3 v{1.0f, 2.0f, 3.0f};
    REQUIRE(approxEqual(Quat::identity().rotate(v), v));
}

TEST_CASE("90-degree rotation about Y maps +Z to +X", "[math][quat]") {
    const Quat q = Quat::fromAxisAngle(Vec3::unitY(), radians(90.0f));
    const Vec3 r = q.rotate(Vec3::unitZ());
    REQUIRE(approxEqual(r, Vec3::unitX(), 1e-5f));
}

TEST_CASE("rotation about Z maps +X to +Y", "[math][quat]") {
    const Quat q = Quat::fromAxisAngle(Vec3::unitZ(), radians(90.0f));
    REQUIRE(approxEqual(q.rotate(Vec3::unitX()), Vec3::unitY(), 1e-5f));
}

TEST_CASE("conjugate/inverse undoes a rotation", "[math][quat]") {
    const Quat q = Quat::fromAxisAngle(normalize(Vec3{1.0f, 1.0f, 0.0f}), radians(57.0f));
    const Vec3 v{2.0f, -1.0f, 0.5f};
    REQUIRE(approxEqual(inverse(q).rotate(q.rotate(v)), v, 1e-4f));
}

TEST_CASE("composition applies right operand first", "[math][quat]") {
    const Quat a = Quat::fromAxisAngle(Vec3::unitY(), radians(90.0f));
    const Quat b = Quat::fromAxisAngle(Vec3::unitZ(), radians(90.0f));
    const Vec3 viaQuat = (a * b).rotate(Vec3::unitX());
    const Vec3 viaSteps = a.rotate(b.rotate(Vec3::unitX()));
    REQUIRE(approxEqual(viaQuat, viaSteps, 1e-5f));
}

TEST_CASE("quaternion-to-matrix agrees with rotate", "[math][quat]") {
    const Quat q = Quat::fromAxisAngle(Vec3::unitX(), radians(30.0f));
    const Mat3 m = toMat3(q);
    const Vec3 v{0.0f, 1.0f, 0.0f};
    REQUIRE(approxEqual(m * v, q.rotate(v), 1e-5f));
}

TEST_CASE("slerp endpoints and midpoint", "[math][quat]") {
    const Quat a = Quat::identity();
    const Quat b = Quat::fromAxisAngle(Vec3::unitY(), radians(90.0f));
    REQUIRE(approxEqual(slerp(a, b, 0.0f), a, 1e-5f));
    REQUIRE(approxEqual(slerp(a, b, 1.0f), b, 1e-5f));
    // Midpoint should be a 45-degree rotation about Y.
    const Quat mid = slerp(a, b, 0.5f);
    const Quat expect = Quat::fromAxisAngle(Vec3::unitY(), radians(45.0f));
    REQUIRE(approxEqual(mid.rotate(Vec3::unitZ()), expect.rotate(Vec3::unitZ()), 1e-4f));
}

TEST_CASE("normalize yields a unit quaternion", "[math][quat]") {
    const Quat q = normalize(Quat{1.0f, 2.0f, 3.0f, 4.0f});
    REQUIRE_THAT(length(q), WithinAbs(1.0f, 1e-6f));
}

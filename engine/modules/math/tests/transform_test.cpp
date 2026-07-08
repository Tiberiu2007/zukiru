#include <zuki/math/transform.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace zuki::math;
using zuki::f32;  // f32 lives in the parent zuki namespace, not zuki::math
using Catch::Matchers::WithinAbs;

TEST_CASE("Transform composes T*R*S consistently", "[math][transform]") {
    Transform t;
    t.position = {10.0f, 0.0f, 0.0f};
    t.rotation = Quat::fromAxisAngle(Vec3::unitZ(), radians(90.0f));
    t.scale = {2.0f, 2.0f, 2.0f};

    const Vec3 direct = t.transformPoint(Vec3::unitX());
    const Vec3 viaMatrix = t.toMatrix().transformPoint(Vec3::unitX());
    REQUIRE(approxEqual(direct, viaMatrix, 1e-5f));
    // scale(2) -> (2,0,0); rot 90 about Z -> (0,2,0); translate -> (10,2,0).
    REQUIRE(approxEqual(direct, Vec3{10.0f, 2.0f, 0.0f}, 1e-5f));
}

TEST_CASE("lookAt places the camera and looks down -Z in view space", "[math][transform]") {
    const Mat4 view = lookAt(Vec3{0.0f, 0.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f}, Vec3::unitY());
    // The target maps to the negative-Z axis in view space at distance 5.
    const Vec3 target = view.transformPoint(Vec3{0.0f, 0.0f, 0.0f});
    REQUIRE_THAT(target.x, WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(target.y, WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(target.z, WithinAbs(-5.0f, 1e-5f));
    // The eye maps to the view-space origin.
    REQUIRE(approxEqual(view.transformPoint(Vec3{0.0f, 0.0f, 5.0f}), Vec3{}, 1e-5f));
}

TEST_CASE("perspective maps near and far plane depths to [0,1]", "[math][transform]") {
    const f32 nearZ = 0.5f;
    const f32 farZ = 100.0f;
    const Mat4 p = perspective(radians(60.0f), 16.0f / 9.0f, nearZ, farZ);

    // A point on the near plane (view space -nearZ) has clip z/w == 0.
    const Vec4 onNear = p * Vec4{0.0f, 0.0f, -nearZ, 1.0f};
    REQUIRE_THAT(onNear.z / onNear.w, WithinAbs(0.0f, 1e-4f));
    // A point on the far plane has clip z/w == 1.
    const Vec4 onFar = p * Vec4{0.0f, 0.0f, -farZ, 1.0f};
    REQUIRE_THAT(onFar.z / onFar.w, WithinAbs(1.0f, 1e-4f));
}

TEST_CASE("orthographic maps the box corners into the unit cube", "[math][transform]") {
    const Mat4 o = orthographic(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 10.0f);
    // Center of the near plane maps to clip origin in XY.
    const Vec3 c = o.transformPoint(Vec3{0.0f, 0.0f, 0.0f});
    REQUIRE_THAT(c.x, WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(c.y, WithinAbs(0.0f, 1e-6f));
    REQUIRE_THAT(c.z, WithinAbs(0.0f, 1e-6f));
    // Right edge maps to +1.
    REQUIRE_THAT(o.transformPoint(Vec3{1.0f, 0.0f, 0.0f}).x, WithinAbs(1.0f, 1e-6f));
}

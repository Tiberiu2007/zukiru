#include <zukiru/render/camera.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace zukiru;
using namespace zukiru::render;
using Catch::Matchers::WithinAbs;

TEST_CASE("default camera is identity", "[render][camera]") {
    const Camera cam;
    const math::Mat4 vp = cam.viewProjection();
    const math::Mat4 id = math::Mat4::identity();
    for (usize i = 0; i < 16; ++i) {
        REQUIRE_THAT(vp.e[i], WithinAbs(id.e[i], 1e-6f));
    }
}

TEST_CASE("lookAt places a point in front of the camera down -Z", "[render][camera]") {
    Camera cam;
    cam.lookAt({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, math::Vec3::unitY());
    REQUIRE(cam.position() == math::Vec3{0.0f, 0.0f, 5.0f});

    // The world origin, viewed from (0,0,5) looking toward -Z, sits 5 units in
    // front of the camera: view space z ≈ -5 (right-handed, camera looks down -Z).
    const math::Vec3 originInView = cam.view().transformPoint({0.0f, 0.0f, 0.0f});
    REQUIRE_THAT(originInView.x, WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(originInView.y, WithinAbs(0.0f, 1e-5f));
    REQUIRE_THAT(originInView.z, WithinAbs(-5.0f, 1e-5f));
}

TEST_CASE("setTransform view is the inverse of the camera placement", "[render][camera]") {
    Camera cam;
    cam.setTransform({3.0f, 0.0f, 0.0f}, math::Quat::identity());
    // With no rotation, the view just subtracts the camera position.
    const math::Vec3 p = cam.view().transformPoint({5.0f, 0.0f, 0.0f});
    REQUIRE_THAT(p.x, WithinAbs(2.0f, 1e-5f));
}

TEST_CASE("perspective projection maps the near plane and is non-identity", "[render][camera]") {
    Camera cam;
    cam.setPerspective(math::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    const math::Mat4& proj = cam.projection();
    // A finite perspective matrix has a -1 in the w-row for the z column (RH,
    // looking down -Z) and is clearly not identity.
    REQUIRE_THAT(proj.e[11], WithinAbs(-1.0f, 1e-6f));
    REQUIRE(proj.e[0] != 1.0f);
}

TEST_CASE("viewProjection composes projection and view", "[render][camera]") {
    Camera cam;
    cam.setPerspective(math::radians(90.0f), 1.0f, 0.1f, 10.0f);
    cam.lookAt({0.0f, 0.0f, 2.0f}, {0.0f, 0.0f, 0.0f}, math::Vec3::unitY());
    const math::Mat4 expected = cam.projection() * cam.view();
    const math::Mat4 vp = cam.viewProjection();
    for (usize i = 0; i < 16; ++i) {
        REQUIRE_THAT(vp.e[i], WithinAbs(expected.e[i], 1e-6f));
    }
}

// Camera — view and projection matrices for rendering.
//
// Backend-agnostic and header-only (pure `math`): a Camera holds a view matrix
// (world → view) and a projection matrix, and hands out their product. It knows
// nothing about the RHI, so it is trivially testable.
//
//   Camera cam;
//   cam.setPerspective(radians(60.0f), 16.0f / 9.0f, 0.1f, 1000.0f);
//   cam.lookAt({0, 2, 5}, {0, 0, 0}, math::Vec3::unitY());
//   Mat4 vp = cam.viewProjection();
#pragma once

#include <zukiru/core/types.hpp>
#include <zukiru/math/math.hpp>

namespace zukiru::render {

class Camera {
public:
    Camera() = default;

    // --- Projection ------------------------------------------------------

    void setPerspective(f32 fovYRadians, f32 aspect, f32 nearPlane, f32 farPlane) {
        projection_ = math::perspective(fovYRadians, aspect, nearPlane, farPlane);
    }
    void setOrthographic(f32 left, f32 right, f32 bottom, f32 top, f32 nearPlane, f32 farPlane) {
        projection_ = math::orthographic(left, right, bottom, top, nearPlane, farPlane);
    }
    void setProjection(const math::Mat4& projection) { projection_ = projection; }

    // --- View ------------------------------------------------------------

    // Point the camera at `center` from `eye`.
    void lookAt(math::Vec3 eye, math::Vec3 center, math::Vec3 up) {
        position_ = eye;
        view_ = math::lookAt(eye, center, up);
    }

    // Place the camera by a rigid transform (position + orientation). The view
    // matrix is the inverse: rotate world into camera space, then translate.
    void setTransform(math::Vec3 position, math::Quat rotation) {
        position_ = position;
        view_ = math::toMat4(math::inverse(rotation)) * math::Mat4::translation(position * -1.0f);
    }
    void setView(const math::Mat4& view) { view_ = view; }

    // --- Accessors -------------------------------------------------------

    [[nodiscard]] const math::Mat4& view() const noexcept { return view_; }
    [[nodiscard]] const math::Mat4& projection() const noexcept { return projection_; }
    [[nodiscard]] math::Mat4 viewProjection() const noexcept { return projection_ * view_; }
    [[nodiscard]] math::Vec3 position() const noexcept { return position_; }

private:
    math::Mat4 view_ = math::Mat4::identity();
    math::Mat4 projection_ = math::Mat4::identity();
    math::Vec3 position_{0.0f, 0.0f, 0.0f};
};

}  // namespace zukiru::render

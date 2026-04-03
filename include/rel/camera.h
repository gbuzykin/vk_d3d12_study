#pragma once

#include "math.h"

namespace app3d::rel {

struct Camera {
    Vec3f eye{0.f, 0.f, 4.f};
    Vec3f center{0.f, 0.f, 0.f};
    Vec3f up{0.f, 1.f, 0.f};
};

class OrbitCameraManipulator {
 public:
    OrbitCameraManipulator(Camera& camera) : camera_(camera) {}

    void startDragging(const Vec2f& p) {
        is_rotating_ = true;
        back0_ = camera_.eye - camera_.center;
        right0_ = normalize(cross(camera_.up, back0_));
        up0_ = normalize(cross(back0_, right0_));
        v0_ = normalize(p.x * right0_ + p.y * up0_ - back0_);
    }

    void stopDragging() { is_rotating_ = false; }

    void drag(const Vec2f& p) {
        if (!is_rotating_) { return; }

        const Vec3f v1 = normalize(p.x * right0_ + p.y * up0_ - back0_);
        const Vec3f rot_axis = cross(v0_, v1);

        const float sensitivity = 200.f;
        const float a = length(rot_axis);
        const auto rotation = a >= 0.001 ? Mat4f::rotate(sensitivity * std::asin(a), (1.f / a) * rot_axis) :
                                           Mat4f::identity();

        camera_.eye = camera_.center + back0_ * rotation;
        camera_.up = up0_ * rotation;
    }

    void move(float delta) {
        const auto dir = camera_.center - camera_.eye;
        camera_.center = camera_.center + delta * dir;
        camera_.eye = camera_.eye + delta * dir;
    }

 private:
    Camera& camera_;

    bool is_rotating_ = false;
    Vec3f v0_{};
    Vec3f back0_{};
    Vec3f right0_{};
    Vec3f up0_{};
};

}  // namespace app3d::rel

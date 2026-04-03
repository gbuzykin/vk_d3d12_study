#pragma once

#include <cmath>
#include <cstdint>

namespace app3d::rel {

struct Vec2f {
    float x, y;
};

struct Vec3f {
    float x, y, z;
};

struct Vec4f {
    float x, y, z, w;
};

constexpr float deg2rad(float a) { return a * 0.017453292529943296f; }
constexpr float rad2deg(float a) { return a * 57.29577951308232f; }
constexpr float length(const Vec3f& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

constexpr Vec3f normalize(const Vec3f& v) {
    const float l = length(v);
    return {v.x / l, v.y / l, v.z / l};
}

constexpr Vec3f operator-(const Vec3f& lhs, const Vec3f& rhs) { return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z}; }
constexpr Vec3f operator+(const Vec3f& lhs, const Vec3f& rhs) { return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z}; }
constexpr Vec3f operator*(const Vec3f& lhs, float rhs) { return {lhs.x * rhs, lhs.y * rhs, lhs.z * rhs}; }
constexpr Vec3f operator*(float lhs, const Vec3f& rhs) { return {lhs * rhs.x, lhs * rhs.y, lhs * rhs.z}; }
constexpr float dot(const Vec3f& lhs, const Vec3f& rhs) { return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z; }
constexpr Vec3f cross(const Vec3f& lhs, const Vec3f& rhs) {
    return {lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x};
}

struct Mat3f {
    float m[3][3];
};

struct Mat4f {
    float m[4][4];

    static constexpr Mat4f zero() {
        return {{{0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}, {0.f, 0.f, 0.f, 0.f}}};
    }

    static constexpr Mat4f identity() {
        return {{{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 0.f, 0.f, 1.f}}};
    }

    static constexpr Mat4f translate(Vec3f t) {
        return {{{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}, {t.x, t.y, t.z, 1.f}}};
    }

    static constexpr Mat4f rotate(float angle, Vec3f axis) {
        const float c = std::cos(deg2rad(angle));
        const float s = std::sin(deg2rad(angle));
        const float one_minus_c = 1.f - c;
        return {{{axis.x * axis.x * one_minus_c + c, axis.x * axis.y * one_minus_c + axis.z * s,
                  axis.x * axis.z * one_minus_c - axis.y * s, 0.f},
                 {axis.y * axis.x * one_minus_c - axis.z * s, axis.y * axis.y * one_minus_c + c,
                  axis.y * axis.z * one_minus_c + axis.x * s, 0.f},
                 {axis.z * axis.x * one_minus_c + axis.y * s, axis.z * axis.y * one_minus_c - axis.x * s,
                  axis.z * axis.z * one_minus_c + c, 0.f},
                 {0.f, 0.f, 0.f, 1.f}}};
    }

    static constexpr Mat4f perspective(float ratio, float fov, float near, float far) {
        const float f = 1.0f / std::tan(deg2rad(0.5f * fov));
        return {{{f / ratio, 0.f, 0.f, 0.f},
                 {0.f, -f, 0.f, 0.f},
                 {0.f, 0.f, far / (near - far), -1.f},
                 {0.f, 0.f, near * far / (near - far), 0.f}}};
    }

    operator Mat3f() const {
        return {{{m[0][0], m[0][1], m[0][2]}, {m[1][0], m[1][1], m[1][2]}, {m[2][0], m[2][1], m[2][2]}}};
    }
};

constexpr Mat4f operator*(const Mat4f& lhs, const Mat4f& rhs) {
    Mat4f result;
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned j = 0; j < 4; ++j) {
            result.m[i][j] = lhs.m[i][0] * rhs.m[0][j] + lhs.m[i][1] * rhs.m[1][j] + lhs.m[i][2] * rhs.m[2][j] +
                             lhs.m[i][3] * rhs.m[3][j];
        }
    }
    return result;
}

}  // namespace app3d::rel

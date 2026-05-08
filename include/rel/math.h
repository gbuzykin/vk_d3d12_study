#pragma once

#include <cmath>

namespace app3d::rel {

struct Vec2f {
    float x, y;
};

struct Vec3f {
    float x, y, z;
    explicit constexpr operator Vec2f() const { return {.x = x, .y = y}; }
};

struct Vec4f {
    float x, y, z, w;
    explicit constexpr operator Vec2f() const { return {.x = x, .y = y}; }
    explicit constexpr operator Vec3f() const { return {.x = x, .y = y, .z = z}; }
};

constexpr float deg2rad(float a) { return a * 0.017453292529943296f; }
constexpr float rad2deg(float a) { return a * 57.29577951308232f; }
inline float length(const Vec3f& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

inline Vec3f normalize(const Vec3f& v) {
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

    static constexpr Mat4f translate(const Vec3f& t) {
        return {{{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}, {t.x, t.y, t.z, 1.f}}};
    }

    static Mat4f rotate(float angle, const Vec3f& axis) {
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

    static Mat4f lookAt(const Vec3f& eye, const Vec3f& center, const Vec3f& updir) {
        const auto back = normalize(eye - center);
        const auto right = normalize(cross(updir, back));
        const auto up = normalize(cross(back, right));

        return {{
            {right.x, up.x, back.x, 0.f},
            {right.y, up.y, back.y, 0.f},
            {right.z, up.z, back.z, 0.f},
            {-dot(eye, right), -dot(eye, up), -dot(eye, back), 1.f},
        }};
    }

    static Mat4f perspective(float ratio, float fov, float near, float far) {
        const float f = 1.0f / std::tan(deg2rad(0.5f * fov));
        return {{{f / ratio, 0.f, 0.f, 0.f},
                 {0.f, f, 0.f, 0.f},
                 {0.f, 0.f, far / (near - far), -1.f},
                 {0.f, 0.f, near * far / (near - far), 0.f}}};
    }

    constexpr Mat4f transpose() const {
        return {{{m[0][0], m[1][0], m[2][0], m[3][0]},
                 {m[0][1], m[1][1], m[2][1], m[3][1]},
                 {m[0][2], m[1][2], m[2][2], m[3][2]},
                 {m[0][3], m[1][3], m[2][3], m[3][3]}}};
    }

    explicit constexpr operator Mat3f() const {
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

constexpr Vec3f operator*(const Vec3f& lhs, const Mat4f& rhs) {
    return {
        lhs.x * rhs.m[0][0] + lhs.y * rhs.m[1][0] + lhs.z * rhs.m[2][0] + rhs.m[3][0],
        lhs.x * rhs.m[0][1] + lhs.y * rhs.m[1][1] + lhs.z * rhs.m[2][1] + rhs.m[3][1],
        lhs.x * rhs.m[0][2] + lhs.y * rhs.m[1][2] + lhs.z * rhs.m[2][2] + rhs.m[3][2],
    };
}

}  // namespace app3d::rel

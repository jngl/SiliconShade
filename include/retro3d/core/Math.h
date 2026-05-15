#ifndef INC_3D_TEST_MATH_H
#define INC_3D_TEST_MATH_H

#include <cinttypes>
#include <cmath>
#include <algorithm>

// --- Types Entiers (utilisés par le rastériseur) ---
struct Vec2 {
    int32_t x, y;
};

struct Vec3 {
    int32_t x, y, z;
};

struct Vec4 {
    int32_t x, y, z, w;
};

// --- Types Float (utilisés pour les transformations) ---
struct Vec2f {
    float x, y;
};

struct Vec3f {
    float x, y, z;

    inline Vec3f operator+(const Vec3f& other) const { return {x + other.x, y + other.y, z + other.z}; }
    inline Vec3f operator-(const Vec3f& other) const { return {x - other.x, y - other.y, z - other.z}; }
    inline Vec3f operator*(float s) const { return {x * s, y * s, z * s}; }

    static inline Vec3f cross(const Vec3f& a, const Vec3f& b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    static inline float dot(const Vec3f& a, const Vec3f& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
};

struct Vec4f {
    float x, y, z, w;
};

struct Mat4 {
    float m[4][4] = {0};

    static inline Mat4 identity() {
        Mat4 res;
        res.m[0][0] = 1.0f; res.m[1][1] = 1.0f; res.m[2][2] = 1.0f; res.m[3][3] = 1.0f;
        return res;
    }

    static inline Mat4 rotate_y(float angle) {
        Mat4 res = identity();
        float c = cosf(angle);
        float s = sinf(angle);
        res.m[0][0] = c;  res.m[0][2] = s;
        res.m[2][0] = -s; res.m[2][2] = c;
        return res;
    }

    inline Vec3f transform(const Vec3f& v) const {
        return {
            v.x * m[0][0] + v.y * m[1][0] + v.z * m[2][0] + m[3][0],
            v.x * m[0][1] + v.y * m[1][1] + v.z * m[2][1] + m[3][1],
            v.x * m[0][2] + v.y * m[1][2] + v.z * m[2][2] + m[3][2]
        };
    }
};

// --- Structures de rendu ---
struct Triangle {
    Vec3 position[3];
    uint32_t color[3]{0, 0, 0};
    Vec2 texcoord[3];
};

struct AABB {
    Vec2 min, max;
};

inline AABB aabbOfTriangle(const Triangle* triangle) {
    AABB aabb;
    aabb.min.x = std::min({triangle->position[0].x, triangle->position[1].x, triangle->position[2].x});
    aabb.min.y = std::min({triangle->position[0].y, triangle->position[1].y, triangle->position[2].y});
    aabb.max.x = std::max({triangle->position[0].x, triangle->position[1].x, triangle->position[2].x});
    aabb.max.y = std::max({triangle->position[0].y, triangle->position[1].y, triangle->position[2].y});
    return aabb;
}

inline AABB aabbIntersect(const AABB* a, const AABB* b)
{
    AABB aabb;
    aabb.min.x = std::max(a->min.x, b->min.x);
    aabb.min.y = std::max(a->min.y, b->min.y);
    aabb.max.x = std::min(a->max.x, b->max.x);
    aabb.max.y = std::min(a->max.y, b->max.y);
    return aabb;
}

#endif //INC_3D_TEST_MATH_H

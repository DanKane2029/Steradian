#pragma once
#include <cmath>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>
using json = nlohmann::json;

/**
 * vector of size three to describe a position or direction in 3D space
 */
struct Vec3
{
    float v[3]{};

    Vec3(float x, float y, float z)
    {
        v[0] = x, v[1] = y, v[2] = z;
    }

    Vec3(std::vector<float> list)
    {
        v[0] = list[0];
        v[1] = list[1];
        v[2] = list[2];
    }

    Vec3(float x)
    {
        v[0] = x, v[1] = x, v[2] = x;
    }

    Vec3()
    {
        v[0] = 0.0f;
        v[1] = 0.0f;
        v[2] = 0.0f;
    }

    auto x() const -> float
    {
        return v[0];
    }

    auto y() const -> float
    {
        return v[1];
    }

    auto z() const -> float
    {
        return v[2];
    }

    // ADD
    inline auto operator+(const Vec3 &vec) -> Vec3
    {
        return {v[0] + vec.v[0], v[1] + vec.v[1], v[2] + vec.v[2]};
    }

    inline void operator+=(const Vec3 &vec)
    {
        v[0] += vec.v[0], v[1] += vec.v[1], v[2] += vec.v[2];
    }

    // SUBTRACT
    inline auto operator-(const Vec3 &vec) -> Vec3
    {
        return {v[0] - vec.v[0], v[1] - vec.v[1], v[2] - vec.v[2]};
    }

    inline void operator-=(const Vec3 &vec)
    {
        v[0] -= vec.v[0], v[1] -= vec.v[1], v[2] -= vec.v[2];
    }

    // MULTIPLY
    inline auto operator*(const Vec3 &vec) -> Vec3
    {
        return {v[0] * vec.v[0], v[1] * vec.v[1], v[2] * vec.v[2]};
    }

    inline void operator*=(const Vec3 &vec)
    {
        v[0] *= vec.v[0], v[1] *= vec.v[1], v[2] *= vec.v[2];
    }

    // SCALE
    inline auto operator*(float s) -> Vec3
    {
        return {v[0] * s, v[1] * s, v[2] * s};
    }

    // DIVIDE
    inline auto operator/(const Vec3 &vec) -> Vec3
    {
        return {v[0] / vec.v[0], v[1] / vec.v[1], v[2] / vec.v[2]};
    }

    inline void operator/=(const Vec3 &vec)
    {
        v[0] /= vec.v[0];
        v[1] /= vec.v[1];
        v[2] /= vec.v[2];
    }

    // DOT PRODUCT
    inline auto dot(const Vec3 &vec) -> float
    {
        return (v[0] * vec.v[0]) + (v[1] * vec.v[1]) + (v[2] * vec.v[2]);
    }

    // CROSS PRODUCT
    inline auto cross(const Vec3 &vec) -> Vec3
    {
        return {v[1] * vec.v[2] - v[2] * vec.v[1], v[2] * vec.v[0] - v[0] * vec.v[2],
                v[0] * vec.v[1] - v[1] * vec.v[0]};
    }

    // LENGTH
    inline auto length() -> float
    {
        return sqrtf((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
    }

    // NORMALIZE
    inline void normalize()
    {
        float length = this->length();
        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
    }

    inline friend auto operator<<(std::ostream &os, const Vec3 &v) -> std::ostream &
    {
        os << "(" << v.x() << ", " << v.y() << ", " << v.z() << ")";
        return os;
    }
};

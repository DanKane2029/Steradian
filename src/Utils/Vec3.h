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
    float x, y, z;

    Vec3(float x, float y, float z)
    {
        this->x = x;
        this->y = y;
        this->z = z;
    }

    Vec3(std::vector<float> list)
    {
        this->x = list[0];
        this->y = list[1];
        this->z = list[2];
    }

    Vec3(float x)
    {
        this->x = x;
        this->y = x;
        this->z = x;
    }

    Vec3()
    {
        this->x = 0.0f;
        this->y = 0.0f;
        this->z = 0.0f;
    }

    // ADD
    inline auto operator+(const Vec3 &vec) -> Vec3
    {
        return {x + vec.x, y + vec.y, z + vec.z};
    }

    inline void operator+=(const Vec3 &vec)
    {
        x += vec.x, y += vec.y, z += vec.z;
    }

    // SUBTRACT
    inline auto operator-(const Vec3 &vec) -> Vec3
    {
        return {x - vec.x, y - vec.y, z - vec.z};
    }

    inline void operator-=(const Vec3 &vec)
    {
        x -= vec.x, y -= vec.y, z -= vec.z;
    }

    // MULTIPLY
    inline auto operator*(const Vec3 &vec) -> Vec3
    {
        return {x * vec.x, y * vec.y, z * vec.z};
    }

    inline void operator*=(const Vec3 &vec)
    {
        x *= vec.x, y *= vec.y, z *= vec.z;
    }

    // SCALE
    inline auto operator*(float s) -> Vec3
    {
        return {x * s, y * s, z * s};
    }

    // DIVIDE
    inline auto operator/(const Vec3 &vec) -> Vec3
    {
        return {x / vec.x, y / vec.y, z / vec.z};
    }

    inline void operator/=(const Vec3 &vec)
    {
        x /= vec.x;
        y /= vec.y;
        z /= vec.z;
    }

    // DOT PRODUCT
    inline auto dot(const Vec3 &vec) -> float
    {
        return (x * vec.x) + (y * vec.y) + (z * vec.z);
    }

    // CROSS PRODUCT
    inline auto cross(const Vec3 &vec) -> Vec3
    {
        return {y * vec.z - z * vec.y, z * vec.x - x * vec.z, x * vec.y - y * vec.x};
    }

    // LENGTH
    inline auto length() -> float
    {
        return sqrtf((x * x) + (y * y) + (z * z));
    }

    // NORMALIZE
    inline void normalize()
    {
        float length = this->length();
        x /= length;
        y /= length;
        z /= length;
    }

    inline friend auto operator<<(std::ostream &os, const Vec3 &v) -> std::ostream &
    {
        os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
        return os;
    }
};

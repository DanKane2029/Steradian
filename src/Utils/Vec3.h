#pragma once
#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <vector>

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

    /**
     * builds a vector from a list, as produced when deserializing a scene file
     *
     * The size is checked: a malformed scene should report a usable error rather than
     * read past the end of the list.
     */
    Vec3(const std::vector<float> &list)
    {
        if (list.size() < 3)
        {
            throw std::invalid_argument("Vec3 needs three components, got " + std::to_string(list.size()));
        }

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
    inline auto operator+(const Vec3 &vec) const -> Vec3
    {
        return {x + vec.x, y + vec.y, z + vec.z};
    }

    inline void operator+=(const Vec3 &vec)
    {
        x += vec.x, y += vec.y, z += vec.z;
    }

    // SUBTRACT
    inline auto operator-(const Vec3 &vec) const -> Vec3
    {
        return {x - vec.x, y - vec.y, z - vec.z};
    }

    inline void operator-=(const Vec3 &vec)
    {
        x -= vec.x, y -= vec.y, z -= vec.z;
    }

    // MULTIPLY
    inline auto operator*(const Vec3 &vec) const -> Vec3
    {
        return {x * vec.x, y * vec.y, z * vec.z};
    }

    inline void operator*=(const Vec3 &vec)
    {
        x *= vec.x, y *= vec.y, z *= vec.z;
    }

    // SCALE
    inline auto operator*(float s) const -> Vec3
    {
        return {x * s, y * s, z * s};
    }

    // SCALAR DIVIDE
    inline auto operator/(float s) const -> Vec3
    {
        const float inv = 1.0f / s;
        return {x * inv, y * inv, z * inv};
    }

    // NEGATE
    inline auto operator-() const -> Vec3
    {
        return {-x, -y, -z};
    }

    // DIVIDE
    inline auto operator/(const Vec3 &vec) const -> Vec3
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
    inline auto dot(const Vec3 &vec) const -> float
    {
        return (x * vec.x) + (y * vec.y) + (z * vec.z);
    }

    // CROSS PRODUCT
    inline auto cross(const Vec3 &vec) const -> Vec3
    {
        return {y * vec.z - z * vec.y, z * vec.x - x * vec.z, x * vec.y - y * vec.x};
    }

    // LENGTH
    inline auto length() const -> float
    {
        return sqrtf((x * x) + (y * y) + (z * z));
    }

    // NORMALIZE
    //
    // Guarded against a zero-length vector, which would otherwise produce NaNs that
    // propagate silently through shading and into the output image.
    inline void normalize()
    {
        const float len = this->length();
        if (len > 0.0f)
        {
            const float inv = 1.0f / len;
            x *= inv;
            y *= inv;
            z *= inv;
        }
    }

    /** returns a unit-length copy, leaving this vector unchanged */
    inline auto normalized() const -> Vec3
    {
        Vec3 result = *this;
        result.normalize();
        return result;
    }

    /** squared length, for comparisons that do not need the square root */
    inline auto lengthSquared() const -> float
    {
        return (x * x) + (y * y) + (z * z);
    }

    inline friend auto operator<<(std::ostream &os, const Vec3 &v) -> std::ostream &
    {
        os << "(" << v.x << ", " << v.y << ", " << v.z << ")";
        return os;
    }
};

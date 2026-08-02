#pragma once
#include "Utils/DeviceCompat.h"

/**
 * \brief A position or direction in 3D space.
 *
 * Compiled both by the host compiler and, unchanged, by NVRTC as device code, which is
 * why it holds nothing but three floats and depends on nothing but DeviceCompat.h. It
 * previously pulled in <iostream>, <vector> and <stdexcept>, and carried a constructor
 * that took a std::vector and threw -- none of which exists on a device.
 *
 * Building one from a JSON array now happens in the scene loader, which is the only place
 * that ever wanted it and the only place that knows what to say when the array is the
 * wrong length.
 */
struct Vec3
{
    float x, y, z;

    PT_HOST_DEVICE Vec3(float x, float y, float z)
    {
        this->x = x;
        this->y = y;
        this->z = z;
    }

    PT_HOST_DEVICE Vec3(float x)
    {
        this->x = x;
        this->y = x;
        this->z = x;
    }

    PT_HOST_DEVICE Vec3()
    {
        this->x = 0.0f;
        this->y = 0.0f;
        this->z = 0.0f;
    }

    // ADD
    inline PT_HOST_DEVICE auto operator+(const Vec3 &vec) const -> Vec3
    {
        return {x + vec.x, y + vec.y, z + vec.z};
    }

    inline PT_HOST_DEVICE void operator+=(const Vec3 &vec)
    {
        x += vec.x, y += vec.y, z += vec.z;
    }

    // SUBTRACT
    inline PT_HOST_DEVICE auto operator-(const Vec3 &vec) const -> Vec3
    {
        return {x - vec.x, y - vec.y, z - vec.z};
    }

    inline PT_HOST_DEVICE void operator-=(const Vec3 &vec)
    {
        x -= vec.x, y -= vec.y, z -= vec.z;
    }

    // MULTIPLY
    inline PT_HOST_DEVICE auto operator*(const Vec3 &vec) const -> Vec3
    {
        return {x * vec.x, y * vec.y, z * vec.z};
    }

    inline PT_HOST_DEVICE void operator*=(const Vec3 &vec)
    {
        x *= vec.x, y *= vec.y, z *= vec.z;
    }

    // SCALE
    inline PT_HOST_DEVICE auto operator*(float s) const -> Vec3
    {
        return {x * s, y * s, z * s};
    }

    // SCALAR DIVIDE
    inline PT_HOST_DEVICE auto operator/(float s) const -> Vec3
    {
        const float inv = 1.0f / s;
        return {x * inv, y * inv, z * inv};
    }

    // NEGATE
    inline PT_HOST_DEVICE auto operator-() const -> Vec3
    {
        return {-x, -y, -z};
    }

    // DIVIDE
    inline PT_HOST_DEVICE auto operator/(const Vec3 &vec) const -> Vec3
    {
        return {x / vec.x, y / vec.y, z / vec.z};
    }

    inline PT_HOST_DEVICE void operator/=(const Vec3 &vec)
    {
        x /= vec.x;
        y /= vec.y;
        z /= vec.z;
    }

    // DOT PRODUCT
    inline PT_HOST_DEVICE auto dot(const Vec3 &vec) const -> float
    {
        return (x * vec.x) + (y * vec.y) + (z * vec.z);
    }

    // CROSS PRODUCT
    inline PT_HOST_DEVICE auto cross(const Vec3 &vec) const -> Vec3
    {
        return {y * vec.z - z * vec.y, z * vec.x - x * vec.z, x * vec.y - y * vec.x};
    }

    // LENGTH
    inline PT_HOST_DEVICE auto length() const -> float
    {
        return sqrtf((x * x) + (y * y) + (z * z));
    }

    // NORMALIZE
    //
    // Guarded against a zero-length vector, which would otherwise produce NaNs that
    // propagate silently through shading and into the output image.
    inline PT_HOST_DEVICE void normalize()
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
    inline PT_HOST_DEVICE auto normalized() const -> Vec3
    {
        Vec3 result = *this;
        result.normalize();
        return result;
    }

    /** squared length, for comparisons that do not need the square root */
    inline PT_HOST_DEVICE auto lengthSquared() const -> float
    {
        return (x * x) + (y * y) + (z * z);
    }
};

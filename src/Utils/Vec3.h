#pragma once
#include <math.h>

/**
 * vector of size three to describe a position or direction in 3D space
 */
struct Vec3
{
	float v[3];

	Vec3(float x, float y, float z)
	{
		v[0] = x, v[1] = y, v[2] = z;
	}

	Vec3(float x)
	{
		v[0] = x, v[1] = x, v[2] = x;
	}

	Vec3()
	{
		v[0] = 0.0f, v[1] = 0.0f, v[2] = 0.0f;
	}

	float X()
	{
		return v[0];
	}

	float Y()
	{
		return v[1];
	}

	float Z()
	{
		return v[2];
	}

	// ADD
	inline Vec3 operator+(const Vec3& vec)
	{
		return Vec3(v[0] + vec.v[0], v[1] + vec.v[1], v[2] + vec.v[2]);
	}

	inline void operator+=(const Vec3& vec)
	{
		v[0] += vec.v[0], v[1] += vec.v[1], v[2] += vec.v[2];
	}

	// SUBTRACT
	inline Vec3 operator-(const Vec3& vec)
	{
		return Vec3(v[0] - vec.v[0], v[1] - vec.v[1], v[2] - vec.v[2]);
	}

	inline void operator-=(const Vec3& vec)
	{
		v[0] -= vec.v[0], v[1] -= vec.v[1], v[2] -= vec.v[2];
	}

	// MULTIPLY
	inline Vec3 operator*(const Vec3& vec)
	{
		return Vec3(v[0] * vec.v[0], v[1] * vec.v[1], v[2] * vec.v[2]);
	}

	inline void operator*=(const Vec3& vec)
	{
		v[0] *= vec.v[0], v[1] *= vec.v[1], v[2] *= vec.v[2];
	}

	// SCALE
	inline Vec3 operator*(float s)
	{
		return Vec3(v[0] * s, v[1] * s, v[2] * s);
	}

	// DIVIDE
	inline Vec3 operator/(const Vec3& vec)
	{
		return Vec3(v[0] / vec.v[0], v[1] / vec.v[1], v[2] / vec.v[2]);
	}

	inline void operator/=(const Vec3& vec)
	{
		v[0] /= vec.v[0], v[1] /= vec.v[1], v[2] /= vec.v[2];
	}

	// DOT PRODUCT
	inline float Dot(const Vec3& vec)
	{
		return (v[0] * vec.v[0]) + (v[1] * vec.v[1]) + (v[2] * vec.v[2]);
	}

	// CROSS PRODUCT
	inline Vec3 Cross(const Vec3& vec)
	{
		return Vec3(
			v[1] * vec.v[2] - v[2] * vec.v[1],
			v[2] * vec.v[0] - v[0] * vec.v[2],
			v[0] * vec.v[1] - v[1] * vec.v[0]
		);
	}

	// LENGTH
	inline float Length()
	{
		return sqrtf((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
	}

	// NORMALIZE
	inline void normalize()
	{
		*this /= Length();
	}
};

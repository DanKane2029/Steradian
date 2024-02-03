#pragma once
#include "Utils/Vec3.h"

/**
 * \brief A point and a direction used to simulate a ray.
 *
 * A struct that contains 2 Vector3 parameters an origin and a direction. Used to calculate ray and scene object
 * intersections.
 */
struct Ray
{
    /**
     * \brief The origin of the ray.
     *
     * A Vector3 used to describe the origin and starting point of the ray.
     */
    Vec3 org;

    /**
     * \brief The direction of the ray.
     *
     * A Vector3 used to describe the direction of the ray. The direction Vector3 should be a unit vector.
     */
    Vec3 dir;

    /**
     * \brief Creates a new Ray.
     *
     * Creates a new Ray with an origin and normalized direction.
     *
     * \param org The origin or starting point of the ray.
     * \param dir The direction of the ray.
     */
    Ray(Vec3 org, Vec3 dir) : org(org), dir(dir)
    {
        this->dir.normalize();
    }

    Ray() : org(Vec3()), dir(Vec3(0, 0, 1))
    {
    }

    /**
     * \brief Gets the position along a ray.
     *
     * Calculates the Vector3 position along a ray given a time parameter.
     *
     * \param t The time value that describes how along the ray the position is where t=0 is the ray origin.
     */
    auto posAt(float t) -> Vec3
    {
        return org + (dir * t);
    }

    /**
     * \brief Reflects the ray based on the hit position and normal.
     */
    Ray getReflectionRay(Vec3 pos, Vec3 normal)
    {
        Vec3 reverseDir = dir * -1.0f;
        float revNDot = reverseDir.dot(normal);
        Vec3 reflectedDir = (normal * 2.0f * revNDot) - reverseDir;
        return Ray(pos, reflectedDir);
    }
};
